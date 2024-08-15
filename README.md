# Tricky Store

A trick of keystore. **Android 12 or above is required**

Shamiko (or similar) is also required for the global props changes and root hiding it provides.

## Usage

1. Flash this module and reboot.  
2. For more than DEVICE integrity, put an unrevoked hardware keybox.xml at `/data/adb/tricky_store/keybox.xml` (Optional).  
3. Customize target packages at `/data/adb/tricky_store/target.txt` (Optional).  
4. Enjoy!  

**All configuration files will take effect immediately.**

## keybox.xml

format:

```xml
<?xml version="1.0"?>
<AndroidAttestation>
    <NumberOfKeyboxes>1</NumberOfKeyboxes>
    <Keybox DeviceID="...">
        <Key algorithm="ecdsa|rsa">
            <PrivateKey format="pem">
-----BEGIN EC PRIVATE KEY-----
...
-----END EC PRIVATE KEY-----
            </PrivateKey>
            <CertificateChain>
                <NumberOfCertificates>...</NumberOfCertificates>
                    <Certificate format="pem">
-----BEGIN CERTIFICATE-----
...
-----END CERTIFICATE-----
                    </Certificate>
                ... more certificates
            </CertificateChain>
        </Key>...
    </Keybox>
</AndroidAttestation>
```

## Support TEE broken devices

Tricky Store will hack the leaf certificate by default. On TEE broken devices, this will not work because we can't retrieve the leaf certificate from TEE. You can add a `!` after a package name to enable generate certificate support for this package.

For example:

```
# target.txt
# use leaf certificate hacking mode for KeyAttestation App
io.github.vvb2060.keyattestation
# use certificate generating mode for gms
com.google.android.gms!
```
## Custom ROMs support

If you are using a custom ROM and it passes Play Integrity (BASIC & DEVICE) by default, there is a good chance that this module won't work for you as your ROM is probably blocking Key Attestation.
To see if your ROM is compatible, look in the `android_frameworks_base` repo of your ROM and search for `PixelPropsUtils` or `setProps`.

To fix this issue, search for `engineGetCertificateChain` in that repo and see if there's some block of code that throws an exception if some condition that checks if it's related to key attestation (e.g. `PixelPropsUtils.getIsKeyAttest()` or `isCallerSafetyNet()`) is filled. You can delete this block of code and build your ROM yourself, or submit a commit to the maintainer of your ROM to add, for example, a system property to enable/disable this blocking. See [this commit](https://github.com/PixelBuildsROM/android_frameworks_base/commit/378ae3b7034d441fd0455639dc1a4b29b2876798) for reference.

## TODO

- Support App Attest Key.
- [Support Android 11 and below.](https://github.com/5ec1cff/TrickyStore/issues/25#issuecomment-2250588463)
- Support automatic selection mode.

PR is welcomed.

## Acknowledgement

- [PlayIntegrityFix](https://github.com/chiteroman/PlayIntegrityFix)
- [FrameworkPatch](https://github.com/chiteroman/FrameworkPatch)
- [BootloaderSpoofer](https://github.com/chiteroman/BootloaderSpoofer)
- [KeystoreInjection](https://github.com/aviraxp/Zygisk-KeystoreInjection)
- [LSPosed](https://github.com/LSPosed/LSPosed)
