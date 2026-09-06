# Superlooper Chance dead-zone test

Image: `build/release/alt-dum-superlooper-v1.0.1-chance10-test.hex`.
Includes the previously tested transport fixes.

Superlooper readings 0-10 now block new mutations. Lowering Chance into this
range also closes any open eligibility window immediately, between clocks.
Existing mutations finish their sampled duration. Readings above 10 retain
the previous probability curve; the classic runner is unchanged.

The Superlooper suite, classic parity check, and Nano Every build pass. New
tests cover all rolled values for readings 0-10, the boundary at 11, midpoint
and maximum Chance, and input-driven window closure/reopening with Split on
and off. The new regression failed before the fix.

Build/upload commands:

```sh
make build FIRMWARE_VERSION=1.0.1-chance10-test BUILD_PATH=build/idum-superlooper-chance10-test
make board-list
make upload BUILD_PATH=build/idum-superlooper-chance10-test PORT=<detected-port>
```

On hardware, test outside LOOP with Chance fully down and clock/trigger inputs
running. Allow any previously active mutation to finish, then confirm that
new mutations stay off. Raise Chance to verify that mutations return, and
lower it again between clocks. If Chance CV is patched, it contributes to the
reading, so check the bottom position with that CV removed as well.

The previous transport test image and its build directory are preserved.
