package io.github.a13e300.tricky_store

import android.content.pm.IPackageManager
import android.os.FileObserver
import android.os.ServiceManager
import io.github.a13e300.tricky_store.keystore.CertHack
import java.io.File

object Config {
    private val hackPackages = mutableSetOf<String>()
    private val generatePackages = mutableSetOf<String>()
    private var isGlobalMode = false
    private var isTeeBrokenMode = false

    private fun updateTargetPackages(f: File?) = runCatching {
        hackPackages.clear()
        generatePackages.clear()
        if (isGlobalMode) {
            Logger.i("Global mode is enabled, skipping updateTargetPackages execution.")
            return@runCatching
        }
        f?.readLines()?.forEach {
            if (it.isNotBlank() && !it.startsWith("#")) {
                val n = it.trim()
                if (isTeeBrokenMode || n.endsWith("!"))
                    generatePackages.add(
                        n.removeSuffix("!").trim()
                    )
                else hackPackages.add(n)
            }
        }
        Logger.i("update hack packages: $hackPackages, generate packages=$generatePackages")
    }.onFailure {
        Logger.e("failed to update target files", it)
    }

    private fun updateKeyBox(f: File?) = runCatching {
        CertHack.readFromXml(f?.readText())
    }.onFailure {
        Logger.e("failed to update keybox", it)
    }

    private fun updateGlobalMode(f: File?) {
        isGlobalMode = f?.exists() == true
        Logger.i("Global mode is ${if (isGlobalMode) "enabled" else "disabled"}")
    }

    private fun updateTeeBrokenMode(f: File?) {
        isTeeBrokenMode = f?.exists() == true
        Logger.i("TEE broken mode is ${if (isTeeBrokenMode) "enabled" else "disabled"}")
    }

    private const val CONFIG_PATH = "/data/adb/tricky_store"
    private const val TARGET_FILE = "target.txt"
    private const val KEYBOX_FILE = "keybox.xml"
    private const val GLOBAL_MODE_FILE = "global_mode"
    private const val TEE_BROKEN_MODE_FILE = "tee_broken_mode"
    private val root = File(CONFIG_PATH)

    object ConfigObserver : FileObserver(root, CLOSE_WRITE or DELETE or MOVED_FROM or MOVED_TO) {
        override fun onEvent(event: Int, path: String?) {
            path ?: return
            val f = when (event) {
                CLOSE_WRITE, MOVED_TO -> File(root, path)
                DELETE, MOVED_FROM -> null
                else -> return
            }
            when (path) {
                TARGET_FILE -> updateTargetPackages(f)
                KEYBOX_FILE -> updateKeyBox(f)
                GLOBAL_MODE_FILE -> {
                    updateGlobalMode(f)
                    updateTargetPackages(File(root, TARGET_FILE))
                }

                TEE_BROKEN_MODE_FILE -> {
                    updateTeeBrokenMode(f)
                    updateTargetPackages(File(root, TARGET_FILE))
                }
            }
        }
    }

    fun initialize() {
        root.mkdirs()
        updateGlobalMode(File(root, GLOBAL_MODE_FILE))
        updateTeeBrokenMode(File(root, TEE_BROKEN_MODE_FILE))
        if (!isGlobalMode) {
            val scope = File(root, TARGET_FILE)
            if (scope.exists()) {
                updateTargetPackages(scope)
            } else {
                Logger.e("target.txt file not found, please put it to $scope !")
            }
        }
        val keybox = File(root, KEYBOX_FILE)
        if (!keybox.exists()) {
            Logger.e("keybox file not found, please put it to $keybox !")
        } else {
            updateKeyBox(keybox)
        }
        ConfigObserver.startWatching()
    }

    private var iPm: IPackageManager? = null

    fun getPm(): IPackageManager? {
        if (iPm == null) {
            iPm = IPackageManager.Stub.asInterface(ServiceManager.getService("package"))
        }
        return iPm
    }

    private fun checkPackages(packages: Set<String>, callingUid: Int) = kotlin.runCatching {
        if (packages.isEmpty()) return false
        val ps = getPm()?.getPackagesForUid(callingUid)
        ps?.any { it in packages }
    }.onFailure { Logger.e("failed to get packages", it) }.getOrNull() ?: false

    fun needHack(callingUid: Int): Boolean {
        return when {
            isTeeBrokenMode -> false
            isGlobalMode -> true
            else -> checkPackages(hackPackages, callingUid)
        }
    }
    
    fun needGenerate(callingUid: Int): Boolean {
        return when {
            isTeeBrokenMode && isGlobalMode -> true
            isGlobalMode -> false
            else -> checkPackages(generatePackages, callingUid)
        }
    }
}