#ifndef V3DVK_MACRO_H
#define V3DVK_MACRO_H

#define anv_printflike(a, b) __attribute__((__format__(__printf__, a, b)))

/**
 * Warn on ignored extension structs.
 *
 * The Vulkan spec requires us to ignore unsupported or unknown structs in
 * a pNext chain.  In debug mode, emitting warnings for ignored structs may
 * help us discover structs that we should not have ignored.
 *
 *
 * From the Vulkan 1.0.38 spec:
 *
 *    Any component of the implementation (the loader, any enabled layers,
 *    and drivers) must skip over, without processing (other than reading the
 *    sType and pNext members) any chained structures with sType values not
 *    defined by extensions supported by that component.
 */
#define v3dvk_debug_ignored_stype(sType) \
   // TODO: Implement intel_logd("%s: ignored VkStructureType %u\n", __func__, (sType))

#endif // V3DVK_MACRO_H
