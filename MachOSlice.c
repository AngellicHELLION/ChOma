#include "MachO.h"
#include "MachOSlice.h"
#include "MachOByteOrder.h"

#include <stdlib.h>

int macho_slice_read_at_offset(MachOSlice *slice, uint64_t offset, size_t size, void *outputBuffer)
{
    return macho_read_at_offset(slice->containingMacho, slice->archDescriptor.offset + offset, size, outputBuffer);
}

int macho_slice_parse_load_commands(MachOSlice *slice)
{
    // Sanity check the number of load commands
    if (slice->machHeader.ncmds < 1 || slice->machHeader.ncmds > 1000) {
        printf("Error: invalid number of load commands (%d).\n", slice->machHeader.ncmds);
        return -1;
    }

    printf("Parsing %d load commands for slice %x/%x.\n", slice->machHeader.ncmds, slice->machHeader.cputype, slice->machHeader.cpusubtype);
    slice->loadCommands = malloc(slice->machHeader.sizeofcmds);

    // Get the offset of the first load command
    uint64_t offset = sizeof(struct mach_header_64);

    // Iterate over all load commands
    for (int j = 0; j < slice->machHeader.ncmds; j++) {
        // Read the load command
        struct load_command loadCommand;
        macho_slice_read_at_offset(slice, offset, sizeof(loadCommand), &loadCommand);
        LOAD_COMMAND_APPLY_BYTE_ORDER(&loadCommand, LITTLE_TO_HOST_APPLIER);

        // Add the load command to the slice
        slice->loadCommands[j] = loadCommand;
        offset += loadCommand.cmdsize;
    }
    return 0;
}

// For one arch of a fat binary
int macho_slice_init_from_fat_arch(MachO *machO, struct fat_arch_64 archDescriptor, MachOSlice *sliceOut)
{
    MachOSlice slice;
    slice.containingMacho = machO;
    slice.archDescriptor = archDescriptor;
    macho_slice_read_at_offset(&slice, 0, sizeof(slice.machHeader), &slice.machHeader);

    // Check the magic against the expected values
    if (slice.machHeader.magic != MH_MAGIC_64 && slice.machHeader.magic != MH_MAGIC) {
        printf("Error: invalid magic 0x%x for mach header at offset 0x%llx.\n", slice.machHeader.magic, archDescriptor.offset);
        return -1;
    }

    // Ensure that the sizeofcmds is a multiple of 8 (it would need padding otherwise)
    if (slice.machHeader.sizeofcmds % 8 != 0) {
        printf("Error: sizeofcmds is not a multiple of 8.\n");
        return -1;
    }

    // Determine if this arch is supported by ChOma
    slice.isSupported = (archDescriptor.cpusubtype != 0x9);
    if (slice.isSupported) {
        // If so, parse it's contents
        macho_slice_parse_load_commands(&slice);
    }

    *sliceOut = slice;
    return 0;
}

// For single arch MachOs
int macho_slice_from_macho(MachO *macho, MachOSlice *sliceOut)
{
    // This function can skip any sanity checks as those will be done by macho_slice_init_from_fat_arch

    struct mach_header_64 machHeader;
    macho_read_at_offset(macho, 0, sizeof(machHeader), &machHeader);
    MACH_HEADER_APPLY_BYTE_ORDER(&machHeader, LITTLE_TO_HOST_APPLIER);

    // Create a FAT arch structure and populate it
    struct fat_arch_64 fakeArch = {0};
    fakeArch.cpusubtype = machHeader.cpusubtype;
    fakeArch.cputype = machHeader.cputype;
    fakeArch.offset = 0;
    fakeArch.size = macho->fileSize;
    fakeArch.align = 0x4000;

    return macho_slice_init_from_fat_arch(macho, fakeArch, sliceOut);
}