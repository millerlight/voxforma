# ==================================================================================================
#
#  Generates a VST3 "moduleinfo.json" manifest for a Windows (MinGW) build.
#
#  JUCE normally produces this file by running a host-side helper binary that is compiled for the
#  *target* platform. When cross-compiling, that helper cannot be executed, so JUCE writes a "{}"
#  placeholder instead. A "{}" manifest is not a valid VST3 module description, and a manifest with
#  the wrong class IDs is worse than none, because the class IDs in a VST3 module are byte-order
#  dependent: the Windows build of a plugin reports different CIDs than the Linux build.
#
#  This script reproduces the JSON that juce_VST3ManifestHelper would emit for a Windows target.
#  The CIDs are computed with exactly the same algorithm as
#  JUCE/modules/juce_audio_processors_headless/utilities/juce_VST3Interface.h
#
#  Usage (script mode):
#     cmake -DVOX_MANIFEST_OUT=<file> -DVOX_MANUFACTURER_CODE=Cltn -DVOX_PLUGIN_CODE=Vxfm
#           -DVOX_NAME=voxforma -DVOX_VERSION=1.0.0 -DVOX_VENDOR=clicktone
#           -DVOX_SUBCATEGORIES=Fx|Filter -DVOX_WINDOWS_BYTE_ORDER=ON
#           -P vst3_moduleinfo.cmake
#

if(NOT DEFINED VOX_WINDOWS_BYTE_ORDER)
    set(VOX_WINDOWS_BYTE_ORDER OFF)
endif()

if(NOT DEFINED VOX_MANIFEST_OUT)
    message(FATAL_ERROR "VOX_MANIFEST_OUT is not set")
endif()

if(NOT DEFINED VOX_MANUFACTURER_CODE OR NOT DEFINED VOX_PLUGIN_CODE)
    message(FATAL_ERROR "VOX_MANUFACTURER_CODE and VOX_PLUGIN_CODE are required")
endif()

# ---------------------------------------------------------------------------------------------
#  Helpers
# ---------------------------------------------------------------------------------------------

# Converts a value in the range 0..255 into two uppercase hex digits.
function(vox_byte out_hex value)
    math(EXPR hi "${value} / 16")
    math(EXPR lo "${value} % 16")

    set(digits "0123456789ABCDEF")
    string(SUBSTRING "${digits}" ${hi} 1 hex_hi)
    string(SUBSTRING "${digits}" ${lo} 1 hex_lo)

    set(${out_hex} "${hex_hi}${hex_lo}" PARENT_SCOPE)
endfunction()

# Converts a four character plugin code (e.g. "Cltn") into the numeric value JUCE uses.
function(vox_code_value out_value code)
    string(LENGTH "${code}" code_len)
    if(NOT code_len EQUAL 4)
        message(FATAL_ERROR "Plugin codes must be exactly four characters, got '${code}'")
    endif()

    set(value 0)
    set(alphabet "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz")
    math(EXPR place 16777216)

    foreach(i RANGE 0 3)
        string(SUBSTRING "${code}" ${i} 1 c)
        string(FIND "${alphabet}" "${c}" digit)

        if(digit EQUAL -1)
            message(FATAL_ERROR "Invalid character '${c}' in plugin code '${code}'")
        endif()

        if(digit LESS 10)
            math(EXPR ascii "${digit} + 48")
        elseif(digit LESS 36)
            math(EXPR ascii "${digit} + 55")
        else()
            math(EXPR ascii "${digit} + 61")
        endif()

        math(EXPR value "${value} + (${ascii} * ${place})")
        math(EXPR place "${place} / 256")
    endforeach()

    set(${out_value} "${value}" PARENT_SCOPE)
endfunction()

# Builds the 16 byte VST3 class id, following juce_VST3Interface.h.
#
# The byte order of a VST3 class id depends on the *target* platform, because
# juce_VST3Interface.h swaps the first two words when JUCE_WINDOWS is defined.
#
#   windows: word0 = byte0, byte1, byte2, byte3 (least significant first)
#            word1 = byte2, byte3, byte0, byte1
#   other:   word0 = byte3, byte2, byte1, byte0 (most significant first)
#            word1 = byte3, byte2, byte1, byte0
#
#   bytes 8..11  : manufacturer code, most significant byte first
#   bytes 12..15 : plugin code, most significant byte first
function(vox_cid out_cid word1 manufacturer_code plugin_code)
    set(word0 0xABCDEF01)

    math(EXPR w0b0 "${word0} & 0xFF")
    math(EXPR w0b1 "(${word0} >> 8) & 0xFF")
    math(EXPR w0b2 "(${word0} >> 16) & 0xFF")
    math(EXPR w0b3 "(${word0} >> 24) & 0xFF")

    math(EXPR w1b0 "${word1} & 0xFF")
    math(EXPR w1b1 "(${word1} >> 8) & 0xFF")
    math(EXPR w1b2 "(${word1} >> 16) & 0xFF")
    math(EXPR w1b3 "(${word1} >> 24) & 0xFF")

    math(EXPR mb0 "(${manufacturer_code} >> 24) & 0xFF")
    math(EXPR mb1 "(${manufacturer_code} >> 16) & 0xFF")
    math(EXPR mb2 "(${manufacturer_code} >> 8) & 0xFF")
    math(EXPR mb3 "${manufacturer_code} & 0xFF")

    math(EXPR pb0 "(${plugin_code} >> 24) & 0xFF")
    math(EXPR pb1 "(${plugin_code} >> 16) & 0xFF")
    math(EXPR pb2 "(${plugin_code} >> 8) & 0xFF")
    math(EXPR pb3 "${plugin_code} & 0xFF")

    vox_byte(h_w0b0 "${w0b0}")
    vox_byte(h_w0b1 "${w0b1}")
    vox_byte(h_w0b2 "${w0b2}")
    vox_byte(h_w0b3 "${w0b3}")
    vox_byte(h_w1b0 "${w1b0}")
    vox_byte(h_w1b1 "${w1b1}")
    vox_byte(h_w1b2 "${w1b2}")
    vox_byte(h_w1b3 "${w1b3}")
    vox_byte(h_mb0 "${mb0}")
    vox_byte(h_mb1 "${mb1}")
    vox_byte(h_mb2 "${mb2}")
    vox_byte(h_mb3 "${mb3}")
    vox_byte(h_pb0 "${pb0}")
    vox_byte(h_pb1 "${pb1}")
    vox_byte(h_pb2 "${pb2}")
    vox_byte(h_pb3 "${pb3}")

    if(VOX_WINDOWS_BYTE_ORDER)
        set(w0_hex "${h_w0b0}${h_w0b1}${h_w0b2}${h_w0b3}")
        set(w1_hex "${h_w1b2}${h_w1b3}${h_w1b0}${h_w1b1}")
    else()
        set(w0_hex "${h_w0b3}${h_w0b2}${h_w0b1}${h_w0b0}")
        set(w1_hex "${h_w1b3}${h_w1b2}${h_w1b1}${h_w1b0}")
    endif()

    set(cid "")
    string(APPEND cid "${w0_hex}")
    string(APPEND cid "${w1_hex}")
    string(APPEND cid "${h_mb0}${h_mb1}${h_mb2}${h_mb3}")
    string(APPEND cid "${h_pb0}${h_pb1}${h_pb2}${h_pb3}")

    set(${out_cid} "${cid}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------------------------

vox_code_value(manufacturer_code "${VOX_MANUFACTURER_CODE}")
vox_code_value(plugin_code "${VOX_PLUGIN_CODE}")

vox_cid(component_cid 0x9182FAEB "${manufacturer_code}" "${plugin_code}")
vox_cid(controller_cid 0x1234ABCD "${manufacturer_code}" "${plugin_code}")

# Build the "Sub Categories" array body, one quoted entry per line.
string(REPLACE "|" ";" subcategory_list "${VOX_SUBCATEGORIES}")
string(REPLACE "," ";" subcategory_list "${subcategory_list}")
set(subcategory_lines "")
foreach(category IN LISTS subcategory_list)
    if(NOT category STREQUAL "")
        string(APPEND subcategory_lines "        \"${category}\",\n")
    endif()
endforeach()

set(manifest
"{
  \"Name\": \"${VOX_NAME}\",
  \"Version\": \"${VOX_VERSION}\",
  \"Factory Info\": {
    \"Vendor\": \"${VOX_VENDOR}\",
    \"URL\": \"\",
    \"E-Mail\": \"\",
    \"Flags\": {
      \"Unicode\": true,
      \"Classes Discardable\": false,
      \"Component Non Discardable\": false,
    },
  },
  \"Classes\": [
    {
      \"CID\": \"${component_cid}\",
      \"Category\": \"Audio Module Class\",
      \"Name\": \"${VOX_NAME}\",
      \"Vendor\": \"${VOX_VENDOR}\",
      \"Version\": \"${VOX_VERSION}\",
      \"SDKVersion\": \"VST 3.8.0\",
      \"Sub Categories\": [
${subcategory_lines}      ],
      \"Class Flags\": 0,
      \"Cardinality\": 2147483647,
      \"Snapshots\": [
      ],
    },
    {
      \"CID\": \"${controller_cid}\",
      \"Category\": \"Component Controller Class\",
      \"Name\": \"${VOX_NAME}\",
      \"Vendor\": \"${VOX_VENDOR}\",
      \"Version\": \"${VOX_VERSION}\",
      \"SDKVersion\": \"VST 3.8.0\",
      \"Sub Categories\": [
${subcategory_lines}      ],
      \"Class Flags\": 0,
      \"Cardinality\": 2147483647,
      \"Snapshots\": [
      ],
    },
  ],
}
")

get_filename_component(manifest_dir "${VOX_MANIFEST_OUT}" DIRECTORY)
file(MAKE_DIRECTORY "${manifest_dir}")
file(WRITE "${VOX_MANIFEST_OUT}" "${manifest}")
message(STATUS "Wrote VST3 manifest to ${VOX_MANIFEST_OUT}")
