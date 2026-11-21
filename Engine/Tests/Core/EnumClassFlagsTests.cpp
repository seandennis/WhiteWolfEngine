#include "Core/EnumClassFlags.h"

#include <doctest/doctest.h>

namespace
{
	enum class ETestFlags : uint8
	{
		None = 0,
		First = 1 << 0,
		Second = 1 << 1,
		Third = 1 << 2,
	};

	WW_ENUM_CLASS_FLAGS(ETestFlags)
}

TEST_SUITE("WW_ENUM_CLASS_FLAGS")
{
	TEST_CASE("Flags combine with | and intersect with &")
	{
		const ETestFlags Combined = ETestFlags::First | ETestFlags::Third;

		CHECK(static_cast<uint8>(Combined) == 5);
		CHECK((Combined & ETestFlags::First) == ETestFlags::First);
		CHECK((Combined & ETestFlags::Second) == ETestFlags::None);
	}

	TEST_CASE("~ inverts the flags")
	{
		const ETestFlags AllButSecond = ~ETestFlags::Second;

		CHECK((AllButSecond & ETestFlags::Second) == ETestFlags::None);
		CHECK((AllButSecond & ETestFlags::First) == ETestFlags::First);
		CHECK((AllButSecond & ETestFlags::Third) == ETestFlags::Third);
	}

	TEST_CASE("|= and &= change a value in place")
	{
		ETestFlags Flags = ETestFlags::First;

		Flags |= ETestFlags::Second;
		CHECK(Flags == (ETestFlags::First | ETestFlags::Second));

		Flags &= ~ETestFlags::First;
		CHECK(Flags == ETestFlags::Second);
	}

	TEST_CASE("EnumHasAnyFlags is true when at least one flag is set")
	{
		const ETestFlags Flags = ETestFlags::First | ETestFlags::Second;

		CHECK(EnumHasAnyFlags(Flags, ETestFlags::First));
		CHECK(EnumHasAnyFlags(Flags, ETestFlags::Second | ETestFlags::Third));
		CHECK_FALSE(EnumHasAnyFlags(Flags, ETestFlags::Third));
		CHECK_FALSE(EnumHasAnyFlags(Flags, ETestFlags::None));
		CHECK_FALSE(EnumHasAnyFlags(ETestFlags::None, ETestFlags::First));
	}

	TEST_CASE("EnumHasAllFlags is true only when every flag is set")
	{
		const ETestFlags Flags = ETestFlags::First | ETestFlags::Second;

		CHECK(EnumHasAllFlags(Flags, ETestFlags::First));
		CHECK(EnumHasAllFlags(Flags, ETestFlags::First | ETestFlags::Second));
		CHECK_FALSE(EnumHasAllFlags(Flags, ETestFlags::First | ETestFlags::Third));
		CHECK(EnumHasAllFlags(Flags, ETestFlags::None));
	}

	TEST_CASE("The operators are usable in constant expressions")
	{
		constexpr ETestFlags Flags = ETestFlags::First | ETestFlags::Second;

		static_assert((Flags & ETestFlags::Second) == ETestFlags::Second);
		static_assert(EnumHasAnyFlags(Flags, ETestFlags::First));
		CHECK(EnumHasAllFlags(Flags, ETestFlags::First | ETestFlags::Second));
	}
}
