#include "HeaderParser.h"

#include "Token.h"
#include "Tokenizer.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <initializer_list>
#include <span>
#include <string>
#include <utility>

namespace
{
	/** One entry of a specifier list: Name, or Name = Value. */
	struct FSpecifier
	{
		std::string Name;

		/** The value without quotes; meaningful only if bHasValue. */
		std::string Value;

		bool bHasValue = false;
	};

	bool IsIdentifier(const FToken& Token)
	{
		return Token.Type == ETokenType::Identifier;
	}

	bool IsIdentifier(const FToken& Token, std::string_view Text)
	{
		return Token.Type == ETokenType::Identifier && Token.Text == Text;
	}

	bool IsPunctuation(const FToken& Token, std::string_view Text)
	{
		return Token.Type == ETokenType::Punctuation && Token.Text == Text;
	}

	bool ContainsIdentifier(std::span<const FToken> Tokens, std::string_view Text)
	{
		return std::ranges::any_of(Tokens, [Text](const FToken& Token) { return IsIdentifier(Token, Text); });
	}

	/** Returns true if one of the strings in Range equals Text. */
	template <typename RangeType>
	bool ContainsText(const RangeType& Range, std::string_view Text)
	{
		return std::ranges::any_of(Range, [Text](const auto& Element) { return std::string_view(Element) == Text; });
	}

	/**
	 * Tracks how deeply a position is nested in parentheses, braces, square brackets and, optionally, the angle
	 * brackets of a template argument list.
	 */
	class FNestingDepth
	{
	public:
		enum class EAngleBrackets : uint8
		{
			/** '<' and '>' are operators, as in an expression. */
			Ignore,

			/** '<' and '>' enclose template arguments, as in a type. */
			Count,
		};

		explicit FNestingDepth(EAngleBrackets InAngleBrackets)
			: AngleBrackets(InAngleBrackets)
		{
		}

		/** Returns true if the position before the next token is not inside any bracket. */
		bool IsAtTopLevel() const
		{
			return Depth == 0;
		}

		/** Accounts for one token. */
		void Advance(const FToken& Token)
		{
			if (Token.Type != ETokenType::Punctuation)
			{
				return;
			}

			const bool bCountsAngles = AngleBrackets == EAngleBrackets::Count;
			const std::string& Text = Token.Text;
			if (Text == "(" || Text == "{" || Text == "[" || (bCountsAngles && Text == "<"))
			{
				++Depth;
			}
			else if (Text == ")" || Text == "}" || Text == "]" || (bCountsAngles && Text == ">"))
			{
				--Depth;
			}
		}

	private:
		EAngleBrackets AngleBrackets;
		int32 Depth = 0;
	};

	/** Returns the index of the first token for which Predicate is true outside every bracket, or Tokens.size(). */
	template <typename PredicateType>
	std::size_t FindAtTopLevel(
		std::span<const FToken> Tokens, FNestingDepth::EAngleBrackets AngleBrackets, PredicateType Predicate)
	{
		FNestingDepth Nesting(AngleBrackets);
		for (std::size_t Index = 0; Index < Tokens.size(); ++Index)
		{
			if (Nesting.IsAtTopLevel() && Predicate(Tokens[Index]))
			{
				return Index;
			}
			Nesting.Advance(Tokens[Index]);
		}

		return Tokens.size();
	}

	/** Parses one header that has already been split into tokens. */
	class FParser
	{
	public:
		explicit FParser(std::vector<FToken> InTokens)
			: Tokens(std::move(InTokens))
		{
		}

		void ParseFile()
		{
			int32 BraceDepth = 0;
			while (!IsAtEnd())
			{
				const FToken& Token = Current();
				if (IsPunctuation(Token, "{"))
				{
					++BraceDepth;
					++Index;
				}
				else if (IsPunctuation(Token, "}"))
				{
					--BraceDepth;
					++Index;
				}
				else if (IsMacroCall("WCLASS"))
				{
					if (BraceDepth > 0)
					{
						AddError(Token.Line,
							"WCLASS() is inside a namespace or another scope; a reflected class must be declared at "
							"global scope");
					}
					ParseClass();
				}
				else if (IsMacroCall("WPROPERTY") || IsMacroCall("WFUNCTION"))
				{
					AddError(Token.Line,
						std::format("{}() is only allowed inside a class that is marked with WCLASS()", Token.Text));
					++Index;
				}
				else
				{
					++Index;
				}
			}
		}

		std::vector<FReflectedClass> Classes;
		std::vector<FParseError> Errors;

	private:
		bool IsAtEnd() const
		{
			return Index >= Tokens.size();
		}

		const FToken& Current() const
		{
			return Tokens[Index];
		}

		/** Returns true if the current token is the identifier Name and the next one is '('. */
		bool IsMacroCall(std::string_view Name) const
		{
			return !IsAtEnd() && IsIdentifier(Current(), Name) && Index + 1 < Tokens.size() &&
				IsPunctuation(Tokens[Index + 1], "(");
		}

		void AddError(int32 Line, std::string Message)
		{
			Errors.push_back(FParseError{.Line = Line, .Message = std::move(Message)});
		}

		/**
		 * Returns the tokens between the '(' at the current position and its matching ')', and moves past the ')'.
		 * Returns false, with an error recorded, if the parentheses are not closed.
		 */
		bool ConsumeParenthesized(std::string_view Owner, int32 Line, std::span<const FToken>& OutTokens)
		{
			const std::size_t Open = Index;
			int32 Depth = 0;
			for (std::size_t Position = Open; Position < Tokens.size(); ++Position)
			{
				if (IsPunctuation(Tokens[Position], "("))
				{
					++Depth;
				}
				else if (IsPunctuation(Tokens[Position], ")"))
				{
					--Depth;
					if (Depth == 0)
					{
						OutTokens = std::span<const FToken>(Tokens).subspan(Open + 1, Position - Open - 1);
						Index = Position + 1;
						return true;
					}
				}
			}

			AddError(Line, std::format("the parentheses of {} are not closed", Owner));
			Index = Tokens.size();
			return false;
		}

		/**
		 * Parses the specifier list of the macro at the current position, "Macro(A, B = "text")", and moves past
		 * it. Returns false, with an error recorded, if the list is malformed.
		 */
		bool ParseSpecifiers(std::vector<FSpecifier>& OutSpecifiers)
		{
			const std::string Macro = Current().Text;
			const int32 Line = Current().Line;
			++Index;

			std::span<const FToken> List;
			if (!ConsumeParenthesized(Macro + "()", Line, List))
			{
				return false;
			}

			std::size_t Position = 0;
			while (Position < List.size())
			{
				if (!IsIdentifier(List[Position]))
				{
					AddError(Line,
						std::format("{}(): expected a specifier name but found '{}'", Macro, List[Position].Text));
					return false;
				}

				FSpecifier Specifier;
				Specifier.Name = List[Position].Text;
				++Position;

				if (Position < List.size() && IsPunctuation(List[Position], "="))
				{
					++Position;
					const bool bIsQuotedText = Position < List.size() && List[Position].Type == ETokenType::String &&
						List[Position].Text.size() >= 2 && List[Position].Text.front() == '"' &&
						List[Position].Text.back() == '"';
					if (!bIsQuotedText)
					{
						AddError(Line,
							std::format("{}(): the value of {} must be text in double quotes", Macro, Specifier.Name));
						return false;
					}

					Specifier.Value = List[Position].Text.substr(1, List[Position].Text.size() - 2);
					Specifier.bHasValue = true;
					++Position;

					if (Specifier.Value.find('\\') != std::string::npos)
					{
						AddError(Line,
							std::format("{}(): the value of {} must not contain a backslash", Macro, Specifier.Name));
						return false;
					}
				}

				if (Position < List.size())
				{
					if (!IsPunctuation(List[Position], ","))
					{
						AddError(Line,
							std::format("{}(): expected ',' after {} but found '{}'",
								Macro,
								Specifier.Name,
								List[Position].Text));
						return false;
					}
					++Position;
					if (Position == List.size())
					{
						AddError(Line, std::format("{}(): there is nothing after the last ','", Macro));
						return false;
					}
				}

				OutSpecifiers.push_back(std::move(Specifier));
			}

			return true;
		}

		/**
		 * Turns specifiers into flag names and a category.
		 *
		 * @param AllowedFlags The specifiers that are flags of this kind of member.
		 * @param ExclusiveFlags Two flags of which a member may have only one.
		 */
		void ApplySpecifiers(std::string_view Macro,
			int32 Line,
			std::span<const FSpecifier> Specifiers,
			std::span<const std::string_view> AllowedFlags,
			const std::array<std::string_view, 2>& ExclusiveFlags,
			std::vector<std::string>& OutFlags,
			std::string& OutCategory)
		{
			bool bHasCategory = false;
			for (const FSpecifier& Specifier : Specifiers)
			{
				if (Specifier.Name == "Category")
				{
					if (!Specifier.bHasValue)
					{
						AddError(Line, std::format("{}(): Category needs a value: Category = \"Name\"", Macro));
					}
					else if (bHasCategory)
					{
						AddError(Line, std::format("{}(): Category is given twice", Macro));
					}
					bHasCategory = true;
					OutCategory = Specifier.Value;
				}
				else if (!ContainsText(AllowedFlags, Specifier.Name))
				{
					AddError(Line, std::format("{}() does not have a specifier named {}", Macro, Specifier.Name));
				}
				else if (Specifier.bHasValue)
				{
					AddError(Line, std::format("{}(): {} does not take a value", Macro, Specifier.Name));
				}
				else if (ContainsText(OutFlags, Specifier.Name))
				{
					AddError(Line, std::format("{}(): {} is given twice", Macro, Specifier.Name));
				}
				else
				{
					OutFlags.push_back(Specifier.Name);
				}
			}

			const bool bHasBothExclusiveFlags =
				ContainsText(OutFlags, ExclusiveFlags[0]) && ContainsText(OutFlags, ExclusiveFlags[1]);
			if (bHasBothExclusiveFlags)
			{
				AddError(Line,
					std::format(
						"{}(): {} and {} cannot be used together", Macro, ExclusiveFlags[0], ExclusiveFlags[1]));
			}
		}

		void ParseClass()
		{
			FReflectedClass Class;
			Class.Line = Current().Line;

			std::vector<FSpecifier> Specifiers;
			if (!ParseSpecifiers(Specifiers))
			{
				return;
			}
			for (const FSpecifier& Specifier : Specifiers)
			{
				AddError(Class.Line, std::format("WCLASS() does not have a specifier named {}", Specifier.Name));
			}

			if (IsAtEnd() || !IsIdentifier(Current(), "class"))
			{
				AddError(Class.Line, "WCLASS() must be followed by a class declaration");
				return;
			}
			++Index;

			if (!ParseClassHead(Class))
			{
				return;
			}

			const bool bIsValidName =
				Class.Name.size() >= 2 && Class.Name[0] == 'W' && Class.Name[1] >= 'A' && Class.Name[1] <= 'Z';
			if (!bIsValidName)
			{
				AddError(Class.Line,
					std::format("the name of the reflected class {} must start with W, as in WDoor", Class.Name));
			}

			ParseClassBody(Class);
			Classes.push_back(std::move(Class));
		}

		/** Parses "Name : public Super" up to the opening brace, and leaves the position on that brace. */
		bool ParseClassHead(FReflectedClass& Class)
		{
			// Everything up to the ':' may be an export macro (ENGINE_API), the name and "final".
			while (!IsAtEnd() && !IsPunctuation(Current(), ":") && !IsPunctuation(Current(), "{") &&
				!IsPunctuation(Current(), ";"))
			{
				if (IsIdentifier(Current()) && Current().Text != "final")
				{
					Class.Name = Current().Text;
				}
				++Index;
			}

			if (Class.Name.empty())
			{
				AddError(Class.Line, "WCLASS() must be followed by a class declaration");
				return false;
			}
			if (IsAtEnd() || IsPunctuation(Current(), ";"))
			{
				AddError(Class.Line,
					std::format("WCLASS() must be followed by the definition of class {}, not a forward declaration",
						Class.Name));
				return false;
			}
			if (IsPunctuation(Current(), "{"))
			{
				AddError(Class.Line,
					std::format(
						"the reflected class {} must derive from WObject or from another reflected class", Class.Name));
				return false;
			}
			++Index;

			if (IsAtEnd() || !IsIdentifier(Current(), "public"))
			{
				AddError(Class.Line, std::format("class {} must derive publicly from its base class", Class.Name));
				return false;
			}
			++Index;

			if (IsAtEnd() || !IsIdentifier(Current()))
			{
				AddError(Class.Line, std::format("class {}: expected the name of the base class", Class.Name));
				return false;
			}
			Class.SuperName = Current().Text;
			++Index;

			const bool bIsSimpleName = !IsAtEnd() && (IsPunctuation(Current(), "{") || IsPunctuation(Current(), ","));
			if (!bIsSimpleName)
			{
				AddError(Class.Line,
					std::format("class {}: the base class must be named plainly, without a namespace or template "
								"arguments",
						Class.Name));
				return false;
			}

			// Further base classes (interfaces) are of no interest here.
			while (!IsAtEnd() && !IsPunctuation(Current(), "{") && !IsPunctuation(Current(), ";"))
			{
				++Index;
			}
			if (IsAtEnd() || IsPunctuation(Current(), ";"))
			{
				AddError(Class.Line, std::format("class {} has no body", Class.Name));
				return false;
			}

			return true;
		}

		/** Parses the body of a class, starting on its opening brace, and moves past its closing brace. */
		void ParseClassBody(FReflectedClass& Class)
		{
			bool bHasDeclareClass = false;
			int32 BraceDepth = 1;
			++Index;

			while (!IsAtEnd() && BraceDepth > 0)
			{
				const FToken& Token = Current();
				const bool bIsDirectMember = BraceDepth == 1;
				if (IsPunctuation(Token, "{"))
				{
					++BraceDepth;
					++Index;
				}
				else if (IsPunctuation(Token, "}"))
				{
					--BraceDepth;
					++Index;
				}
				else if (IsMacroCall("WCLASS") ||
					(!bIsDirectMember && (IsMacroCall("WPROPERTY") || IsMacroCall("WFUNCTION"))))
				{
					AddError(Token.Line,
						std::format(
							"{}() is inside a nested scope of class {}; it must be used directly in a class body",
							Token.Text,
							Class.Name));
					++Index;
				}
				else if (IsMacroCall("WW_DECLARE_CLASS") && bIsDirectMember)
				{
					ParseDeclareClass(Class);
					bHasDeclareClass = true;
				}
				else if (IsMacroCall("WPROPERTY"))
				{
					ParseProperty(Class);
				}
				else if (IsMacroCall("WFUNCTION"))
				{
					ParseFunction(Class);
				}
				else
				{
					++Index;
				}
			}

			if (BraceDepth > 0)
			{
				AddError(Class.Line, std::format("the body of class {} is not closed", Class.Name));
			}
			if (!bHasDeclareClass)
			{
				AddError(Class.Line,
					std::format("class {} needs WW_DECLARE_CLASS({}, {}) as the first line of its body",
						Class.Name,
						Class.Name,
						Class.SuperName));
			}
		}

		void ParseDeclareClass(const FReflectedClass& Class)
		{
			const int32 Line = Current().Line;
			++Index;

			std::span<const FToken> Arguments;
			if (!ConsumeParenthesized("WW_DECLARE_CLASS()", Line, Arguments))
			{
				return;
			}

			const bool bMatches = Arguments.size() == 3 && IsIdentifier(Arguments[0], Class.Name) &&
				IsPunctuation(Arguments[1], ",") && IsIdentifier(Arguments[2], Class.SuperName);
			if (!bMatches)
			{
				AddError(Line,
					std::format("WW_DECLARE_CLASS must name the class and its base class: WW_DECLARE_CLASS({}, {})",
						Class.Name,
						Class.SuperName));
			}
		}

		/**
		 * Returns the tokens from the current position up to, but not including, the first top-level token for
		 * which IsTerminator is true, and moves onto that token. Returns false if the class body or the file ends
		 * first.
		 */
		template <typename PredicateType>
		bool ConsumeDeclaration(PredicateType IsTerminator, std::span<const FToken>& OutTokens)
		{
			const std::size_t Start = Index;
			FNestingDepth Nesting(FNestingDepth::EAngleBrackets::Ignore);
			for (std::size_t Position = Start; Position < Tokens.size(); ++Position)
			{
				const FToken& Token = Tokens[Position];
				if (Nesting.IsAtTopLevel())
				{
					if (IsTerminator(Token))
					{
						OutTokens = std::span<const FToken>(Tokens).subspan(Start, Position - Start);
						Index = Position;
						return true;
					}
					if (IsPunctuation(Token, "}"))
					{
						return false;
					}
				}
				Nesting.Advance(Token);
			}

			return false;
		}

		void ParseProperty(FReflectedClass& Class)
		{
			FReflectedProperty Property;
			Property.Line = Current().Line;

			std::vector<FSpecifier> Specifiers;
			if (!ParseSpecifiers(Specifiers))
			{
				return;
			}

			static constexpr std::array<std::string_view, 3> AllowedFlags{
				"Transient", "BlueprintReadOnly", "BlueprintReadWrite"};
			static constexpr std::array<std::string_view, 2> ExclusiveFlags{"BlueprintReadOnly", "BlueprintReadWrite"};
			ApplySpecifiers("WPROPERTY",
				Property.Line,
				Specifiers,
				AllowedFlags,
				ExclusiveFlags,
				Property.Flags,
				Property.Category);

			std::span<const FToken> Declaration;
			const bool bFoundEnd =
				ConsumeDeclaration([](const FToken& Token) { return IsPunctuation(Token, ";"); }, Declaration);
			if (!bFoundEnd)
			{
				AddError(Property.Line, "WPROPERTY() must be followed by a member variable that ends with ';'");
				return;
			}

			if (!ParseVariableName(Property.Line, Declaration, Property.Name))
			{
				return;
			}

			const bool bIsDuplicate = std::ranges::any_of(Class.Properties,
				[&Property](const FReflectedProperty& Existing) { return Existing.Name == Property.Name; });
			if (bIsDuplicate)
			{
				AddError(
					Property.Line, std::format("class {} already has a property named {}", Class.Name, Property.Name));
				return;
			}

			Class.Properties.push_back(std::move(Property));
		}

		/** Finds the name in the declaration of a member variable, "Type Name = Initializer". */
		bool ParseVariableName(int32 Line, std::span<const FToken> Declaration, std::string& OutName)
		{
			using EAngleBrackets = FNestingDepth::EAngleBrackets;

			if (ContainsIdentifier(Declaration, "WPROPERTY") || ContainsIdentifier(Declaration, "WFUNCTION"))
			{
				AddError(Line, "WPROPERTY() must be followed by a member variable that ends with ';'");
				return false;
			}
			if (ContainsIdentifier(Declaration, "static"))
			{
				AddError(Line, "a static member variable cannot be a WPROPERTY()");
				return false;
			}

			// The name is the last token before the initializer, if there is one.
			const std::size_t NameEnd = FindAtTopLevel(Declaration,
				EAngleBrackets::Count,
				[](const FToken& Token) { return IsPunctuation(Token, "=") || IsPunctuation(Token, "{"); });
			const std::span<const FToken> Declarator = Declaration.first(NameEnd);

			const std::size_t FirstParenthesis = FindAtTopLevel(
				Declarator, EAngleBrackets::Count, [](const FToken& Token) { return IsPunctuation(Token, "("); });
			if (FirstParenthesis != Declarator.size())
			{
				AddError(Line, "WPROPERTY() must be followed by a member variable; mark a function with WFUNCTION()");
				return false;
			}

			const std::size_t FirstColon = FindAtTopLevel(
				Declarator, EAngleBrackets::Count, [](const FToken& Token) { return IsPunctuation(Token, ":"); });
			if (FirstColon != Declarator.size())
			{
				AddError(Line, "a bit-field cannot be a WPROPERTY()");
				return false;
			}

			if (!Declarator.empty() && IsPunctuation(Declarator.back(), "]"))
			{
				AddError(Line, "a C array cannot be a WPROPERTY(); use std::vector");
				return false;
			}

			const bool bHasTypeAndName = Declarator.size() >= 2 && IsIdentifier(Declarator.back());
			if (!bHasTypeAndName)
			{
				AddError(Line, "WPROPERTY() must be followed by a member variable: a type and a name");
				return false;
			}

			OutName = Declarator.back().Text;
			return true;
		}

		void ParseFunction(FReflectedClass& Class)
		{
			FReflectedFunction Function;
			Function.Line = Current().Line;

			std::vector<FSpecifier> Specifiers;
			if (!ParseSpecifiers(Specifiers))
			{
				return;
			}

			static constexpr std::array<std::string_view, 2> AllowedFlags{"BlueprintCallable", "BlueprintPure"};
			static constexpr std::array<std::string_view, 2> ExclusiveFlags{"BlueprintCallable", "BlueprintPure"};
			ApplySpecifiers("WFUNCTION",
				Function.Line,
				Specifiers,
				AllowedFlags,
				ExclusiveFlags,
				Function.Flags,
				Function.Category);

			// The declaration ends at ';' or, for a function defined in the class, at the '{' of its body. The
			// body is left for the caller to skip.
			std::span<const FToken> Declaration;
			const bool bFoundEnd = ConsumeDeclaration([](const FToken& Token)
				{ return IsPunctuation(Token, ";") || IsPunctuation(Token, "{"); },
				Declaration);
			if (!bFoundEnd)
			{
				AddError(Function.Line, "WFUNCTION() must be followed by a member function");
				return;
			}

			if (!ParseFunctionSignature(Declaration, Function))
			{
				return;
			}

			const bool bIsDuplicate = std::ranges::any_of(Class.Functions,
				[&Function](const FReflectedFunction& Existing) { return Existing.Name == Function.Name; });
			if (bIsDuplicate)
			{
				AddError(Function.Line,
					std::format("class {} already has a reflected function named {}; overloaded functions cannot be "
								"reflected",
						Class.Name,
						Function.Name));
				return;
			}

			Class.Functions.push_back(std::move(Function));
		}

		/** Finds the name and the parameter names in "ReturnType Name(Type A, Type B = Default) const". */
		bool ParseFunctionSignature(std::span<const FToken> Declaration, FReflectedFunction& Function)
		{
			using EAngleBrackets = FNestingDepth::EAngleBrackets;
			const int32 Line = Function.Line;

			if (ContainsIdentifier(Declaration, "WPROPERTY") || ContainsIdentifier(Declaration, "WFUNCTION"))
			{
				AddError(Line, "WFUNCTION() must be followed by a member function");
				return false;
			}
			if (ContainsIdentifier(Declaration, "operator"))
			{
				AddError(Line, "an operator cannot be a WFUNCTION()");
				return false;
			}

			const std::size_t ParametersOpen = FindAtTopLevel(
				Declaration, EAngleBrackets::Count, [](const FToken& Token) { return IsPunctuation(Token, "("); });
			const bool bHasNameAndParameters = ParametersOpen != Declaration.size() && ParametersOpen >= 1 &&
				IsIdentifier(Declaration[ParametersOpen - 1]);
			if (!bHasNameAndParameters)
			{
				AddError(Line, "WFUNCTION() must be followed by a member function; mark a variable with WPROPERTY()");
				return false;
			}

			const std::span<const FToken> BeforeName = Declaration.first(ParametersOpen - 1);
			Function.Name = Declaration[ParametersOpen - 1].Text;
			if (BeforeName.empty())
			{
				AddError(Line,
					std::format(
						"{} has no return type; a constructor or destructor cannot be a WFUNCTION()", Function.Name));
				return false;
			}
			for (const std::string_view Keyword : {"static", "template", "friend"})
			{
				if (ContainsIdentifier(BeforeName, Keyword))
				{
					AddError(Line, std::format("a {} function cannot be a WFUNCTION()", Keyword));
					return false;
				}
			}

			// Find the ')' that closes the parameter list.
			const std::span<const FToken> FromOpen = Declaration.subspan(ParametersOpen);
			std::size_t ParametersLength = FromOpen.size();
			{
				FNestingDepth Nesting(EAngleBrackets::Ignore);
				for (std::size_t Position = 0; Position < FromOpen.size(); ++Position)
				{
					Nesting.Advance(FromOpen[Position]);
					if (Nesting.IsAtTopLevel())
					{
						ParametersLength = Position;
						break;
					}
				}
			}
			if (ParametersLength == FromOpen.size())
			{
				AddError(Line, std::format("the parameter list of {} is not closed", Function.Name));
				return false;
			}

			const std::span<const FToken> Parameters = FromOpen.subspan(1, ParametersLength - 1);
			const bool bIsEmptyList =
				Parameters.empty() || (Parameters.size() == 1 && IsIdentifier(Parameters[0], "void"));
			if (bIsEmptyList)
			{
				return true;
			}

			std::span<const FToken> Remaining = Parameters;
			while (true)
			{
				const std::size_t CommaIndex = FindAtTopLevel(
					Remaining, EAngleBrackets::Count, [](const FToken& Token) { return IsPunctuation(Token, ","); });

				std::string ParameterName;
				const int32 ParameterNumber = static_cast<int32>(Function.ParameterNames.size()) + 1;
				if (!ParseParameterName(
						Line, Function.Name, ParameterNumber, Remaining.first(CommaIndex), ParameterName))
				{
					return false;
				}
				Function.ParameterNames.push_back(std::move(ParameterName));

				if (CommaIndex == Remaining.size())
				{
					return true;
				}
				Remaining = Remaining.subspan(CommaIndex + 1);
			}
		}

		/** Finds the name in one parameter, "const Type& Name = Default". */
		bool ParseParameterName(int32 Line,
			std::string_view FunctionName,
			int32 ParameterNumber,
			std::span<const FToken> Parameter,
			std::string& OutName)
		{
			using EAngleBrackets = FNestingDepth::EAngleBrackets;

			const std::size_t DefaultIndex = FindAtTopLevel(
				Parameter, EAngleBrackets::Count, [](const FToken& Token) { return IsPunctuation(Token, "="); });
			const std::span<const FToken> Declarator = Parameter.first(DefaultIndex);

			if (!Declarator.empty() && IsPunctuation(Declarator.back(), "]"))
			{
				AddError(Line,
					std::format("parameter {} of {} is a C array, which cannot be reflected; use std::vector",
						ParameterNumber,
						FunctionName));
				return false;
			}

			if (!HasParameterName(Declarator))
			{
				AddError(Line,
					std::format("parameter {} of {} needs a name; it becomes the name of the pin in a Blueprint",
						ParameterNumber,
						FunctionName));
				return false;
			}

			OutName = Declarator.back().Text;
			return true;
		}

		/** Decides whether the last token of a parameter is its name rather than the end of its type. */
		static bool HasParameterName(std::span<const FToken> Declarator)
		{
			static constexpr std::array<std::string_view, 12> BuiltInTypes{"void",
				"bool",
				"char",
				"short",
				"int",
				"long",
				"float",
				"double",
				"signed",
				"unsigned",
				"auto",
				"wchar_t"};
			static constexpr std::array<std::string_view, 6> TypeIntroducers{
				"const", "volatile", "struct", "class", "enum", "typename"};

			if (Declarator.size() < 2 || !IsIdentifier(Declarator.back()))
			{
				return false;
			}

			// "int": the last token is a type keyword. "std::string": it is the last part of a qualified name.
			const FToken& Last = Declarator.back();
			const FToken& BeforeLast = Declarator[Declarator.size() - 2];
			if (ContainsText(BuiltInTypes, Last.Text) || IsPunctuation(BeforeLast, "::"))
			{
				return false;
			}

			// "const FVector": everything before the last token only introduces a type, so the last token is the type.
			const std::span<const FToken> BeforeLastTokens = Declarator.first(Declarator.size() - 1);
			const bool bLastIsTheType = std::ranges::all_of(BeforeLastTokens,
				[](const FToken& Token) { return IsIdentifier(Token) && ContainsText(TypeIntroducers, Token.Text); });
			return !bLastIsTheType;
		}

		std::vector<FToken> Tokens;
		std::size_t Index = 0;
	};
}

bool FHeaderParser::Parse(
	std::string_view Source, std::vector<FReflectedClass>& OutClasses, std::vector<FParseError>& OutErrors)
{
	FParser Parser(FTokenizer::Tokenize(Source));
	Parser.ParseFile();

	OutErrors = std::move(Parser.Errors);
	if (!OutErrors.empty())
	{
		OutClasses.clear();
		return false;
	}

	OutClasses = std::move(Parser.Classes);
	return true;
}
