# Compiler settings shared by every first-party target.
#
# Third-party targets never receive these settings: their headers are consumed as SYSTEM
# includes, so their warnings do not break our warnings-as-errors builds.

# Applies the project's warning level and conformance settings to a first-party target.
function(ww_apply_compiler_settings target)
    # Adds /WX on MSVC and -Werror on GCC and Clang.
    set_target_properties(${target} PROPERTIES COMPILE_WARNING_AS_ERROR ON)

    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-          # Strict standard conformance.
            /utf-8                # Source and execution character sets are UTF-8.
            /Zc:preprocessor      # Standard-conforming preprocessor.
            /Zc:__cplusplus       # Report the real language version in __cplusplus.
            /MP                   # Compile the files of a project in parallel.
            # C4251 and C4275 warn that a DLL-exported class uses a type without a DLL interface
            # (in practice, standard library types). That is safe here because every module is
            # built with the same compiler and shares one C runtime; see docs/DECISIONS.md, D-003.
            /wd4251
            /wd4275)
        target_compile_definitions(${target} PRIVATE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
            UNICODE
            _UNICODE)
    else()
        # Chosen to flag on GCC and Clang what MSVC reports at /W4, so development builds on
        # those compilers predict the MSVC result as closely as possible.
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wconversion
            -Wsign-conversion
            -Wold-style-cast
            -Wnon-virtual-dtor
            -Woverloaded-virtual
            -Wimplicit-fallthrough
            -Wundef
            -Wextra-semi)
    endif()
endfunction()

# Enables the given sanitizers for everything configured after this call, third-party code
# included. Development aid for GCC and Clang; does nothing when the list is empty.
function(ww_enable_sanitizers sanitizers)
    if(NOT sanitizers)
        return()
    endif()
    if(MSVC)
        message(FATAL_ERROR "WW_SANITIZERS is only supported with GCC and Clang.")
    endif()

    list(JOIN sanitizers "," sanitizer_list)
    add_compile_options(-fsanitize=${sanitizer_list} -fno-omit-frame-pointer -fno-sanitize-recover=all)
    add_link_options(-fsanitize=${sanitizer_list})
    message(STATUS "White Wolf Engine: sanitizers enabled: ${sanitizer_list}")
endfunction()
