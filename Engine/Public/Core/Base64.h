#pragma once

#include "CoreMinimal.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/**
 * Base64 text (RFC 4648): bytes written with 64 letters, as a glTF file embeds a picture in a "data:" address and as
 * the agent interface hands a picture to an agent.
 */
class FBase64
{
public:
	/** Returns bytes as base64 text, in the standard alphabet and padded with '=' to a multiple of four letters. */
	ENGINE_API static std::string Encode(std::span<const std::byte> Bytes);

	/**
	 * Returns the bytes that base64 text stands for. Both the standard alphabet and the one for addresses, with '-' and
	 * '_' in place of '+' and '/', are read, the padding may be left out, and what follows the first '=' is ignored.
	 *
	 * @return The bytes, or nothing if the text has a character that is in neither alphabet, such as a space.
	 */
	ENGINE_API static std::optional<std::vector<std::byte>> Decode(std::string_view Text);
};
