# bootloader/spacemit/optee_os: android16-riscv

Changes made for the Android 16 (AOSP, riscv64) bring-up of the BananaPi BPI-F3 (SpacemiT K1) and the BananaPi BPI-SM10 (SpacemiT K3), on branch `android16-riscv`.

OP-TEE OS for the SpacemiT K1, on top of the RISE RISC-V OP-TEE branch `dev-optee-mpxy-v8` of https://gitlab.com/riseproject/riscv-optee/optee_os (OP-TEE in an OpenSBI trusted domain, SBI MPXY/RPMI transport).

## Changes

- **core: riscv: add the SpacemiT K1 platform**: BananaPi BPI-F3 / MusePi Pro: OP-TEE runs in an OpenSBI trusted domain (secure DDR at 0x36000000, 32 MiB) and talks to Linux over SBI MPXY/RPMI. 8 harts, 24 MHz time base, console through OpenSBI (DBCN) so the UART keeps a single owner, software PRNG, 256 KiB core heap for the per-channel MPXY notification buffers; no secure interrupts (the PLIC S-mode contexts belong to Linux), so every interrupt is foreign.
- **core: riscv: core_mmu_arch: zero-initialize new page tables**: New page table pages must always start cleared. On some platforms (e.g., QEMU) RAM happens to be zeroed at reset, but on real hardware (FPGA/SoC DDR) may not be the case. Without this memset, stale contents can make core_mmu_map_region() see non-zero old_attr and panic with "Page is already mapped" when CFG_DYN_CONFIG is enabled.
- **core: riscv: make new translations visible without Svvptc**: core_mmu_map_pages() and friends only call core_mmu_table_write_barrier() after turning invalid PTEs into valid ones, on the ARM assumption that invalid entries are never cached. RISC-V only guarantees that with Svvptc; without it the new mapping may stay invisible until sfence.vma. QEMU never caches invalid entries, the SpacemiT K1 (X60) does: the first thread stack mapped by virt_page_alloc() took a store page fault in init_canaries().
- **core: riscv: bring the secondary harts up one at a time**: start_secondary_cores() skipped any hart that was not yet STOPPED (still in its OpenSBI warm boot) and started all the others back to back, so several harts went through the OpenSBI domain context switch at once. On the 8-hart SpacemiT K1 that left harts 5/6 never started and harts 2/3 started but unusable by Linux. Wait (up to 1 s) for each hart to be STOPPED, start it, and wait until it has left the trusted domain before starting the next one.
- **core: pta: add a verified boot root of trust PTA**: CFG_BOOT_ROT_PTA keeps the Android verified boot root of trust (vbmeta key and digest, lock and boot state) in secure memory for the current boot. The bootloader sets it once through the normal world, before any TA reads it; only TAs (KeyMint) can read it. Unlike the AVB TA persistent values, it needs no secure storage, which U-Boot cannot reach without RPMB.
- **plat-spacemit: enable the boot root of trust PTA**: U-Boot sets it after AVB for the KeyMint TA.

## Notes

- Verified boot root of trust: U-Boot (pi-u-boot, OP-TEE client over SBI MPXY) sets it after AVB in the boot_rot PTA (`lib/libutee/include/pta_boot_rot.h`, UUID 368a3590-0d0c-400d-8bd5-35cb3785e435); the KeyMint TA reads it. Write-once per boot: the normal world cannot change it after U-Boot set it or a TA read it.
- Boot flow: SPL loads `tee.bin` from u-boot.itb to 0x36000000; OpenSBI (bootloader/spacemit/opensbi) starts it in the trusted domain declared by pi-u-boot's k1-x-optee.dtsi; OP-TEE initializes the 8 harts one at a time, then OpenSBI boots U-Boot in the untrusted domain.
- Not production ready: software PRNG, no hardware unique key, no secure interrupts, no IOPMP (DMA masters can reach the secure region).
- Two fixes are generic RISC-V real-hardware fixes worth upstreaming: the `sfence.vma` after new translations and the serialized secondary start; the page-table zeroing is upstream 42f39b523d22.

## Build

```
./build.sh k1 --bootloader-only   # builds OP-TEE, stages tee.bin into u-boot.itb
```
