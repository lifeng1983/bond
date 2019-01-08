// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include <bond/core/config.h>

#include "detail/string_stream.h"

#include <bond/core/bond_types.h>

#include <boost/locale/encoding_utf.hpp>
#include <boost/utility/enable_if.hpp>

#define BOND_THROW(x, y)  throw x((::bond::detail::basic_string_stream<1024>() << y).content());

namespace bond
{

/// @brief Base type for all Bond exceptions.
class Exception
    : public std::exception,
      public SerializableExceptionBase
{
public:
    const char* what() const BOND_NOEXCEPT
    {
        return message.c_str();
    }

    virtual ~Exception() BOND_NOEXCEPT
    {}

protected:
    Exception(const char* msg) BOND_NOEXCEPT
    {
        message = msg;
    }

    Exception() BOND_NOEXCEPT
    {}
};


/// @brief %Exception used to indicate an error during serialization or
/// deserialization.
struct CoreException
    : Exception
{
    CoreException(const char* message)
        : Exception(message)
    {}
};


BOND_NORETURN inline void MergerContainerException(uint32_t payload, uint32_t obj)
{
    BOND_THROW(CoreException,
          "Merge failed: container mismatch, length in the payload: "
          << payload << " length in the object: " << obj);
}


BOND_NORETURN inline void InvalidKeyTypeException()
{
    BOND_THROW(CoreException,
        "Map key type not valid");
}


namespace detail
{
    template <typename Key>
    BOND_NORETURN inline void ElementNotFoundExceptionHelper(
        const Key& key,
        typename boost::enable_if<is_wstring<Key>>::type* = nullptr)
    {
        try
        {
            BOND_THROW(CoreException,
                "Map element not found: key: " <<
                    boost::locale::conv::utf_to_utf<char>(
                        string_data(key),
                        string_data(key) + string_length(key),
                        boost::locale::conv::stop));
        }
        catch (boost::locale::conv::conversion_error &)
        {
            BOND_THROW(CoreException, "Map element not found: key: <bad wstring>");
        }
    }

    template <typename Key>
    BOND_NORETURN inline void ElementNotFoundExceptionHelper(
        const Key& key,
        typename boost::disable_if<is_wstring<Key>>::type* = nullptr)
    {
        BOND_THROW(CoreException,
            "Map element not found: key: " << key);
    }
}


template <typename Key>
BOND_NORETURN inline void ElementNotFoundException(const Key& key)
{
    detail::ElementNotFoundExceptionHelper(key);
}


BOND_NORETURN inline void UnknownProtocolException()
{
    BOND_THROW(CoreException,
          "Unmarshaling failed: unsupported protocol");
}


BOND_NORETURN inline void UnknownProtocolException(uint16_t magic)
{
    BOND_THROW(CoreException,
          "Unsupported protocol: "
          << (char)(magic & 0xFF) << (char)(magic >> 8));
}


BOND_NORETURN inline void NothingException()
{
    BOND_THROW(CoreException,
          "Field value is 'nothing'");
}


BOND_NORETURN inline void InvalidEnumValueException(const char* value, const char* enum_)
{
    BOND_THROW(bond::CoreException,
        "Unexpected value " << value << " for enum " << enum_);
}


BOND_NORETURN inline void InvalidEnumValueException(int32_t value, const char* enum_)
{
    BOND_THROW(bond::CoreException,
        "Unexpected value " << value << " for enum " << enum_);
}


BOND_NORETURN inline void RapidJsonException(const char* error, size_t offset)
{
    BOND_THROW(CoreException,
        "JSON parser error: " << error << " at offset " << offset);
}


BOND_NORETURN inline void UnicodeConversionException()
{
    BOND_THROW(CoreException,
        "Unicode conversion exception");
}


struct StreamException
    : Exception
{
    StreamException(const char* message)
        : Exception(message)
    {}
};


struct SchemaValidateException
    : CoreException
{
    SchemaValidateException(const char* message)
        : CoreException(message)
    {}
};


BOND_NORETURN
inline void StructBaseDifferentException(const StructDef& src,
                                         const StructDef& dst)
{
    BOND_THROW(SchemaValidateException,
        "Schemas are incompatible; struct base different: "
        << src.metadata.name << ", " << dst.metadata.name);
}


BOND_NORETURN
inline void RequiredFieldMissingException(const StructDef& s_dst,
                                          const FieldDef& f_dst)
{
    BOND_THROW(SchemaValidateException,
        "Schemas are incompatible; required field missing: "
        << s_dst.metadata.name << "::" << f_dst.metadata.name);
}


BOND_NORETURN
inline void OptionalToRequiredException(const StructDef& s_src,
                                        const StructDef& s_dst,
                                        const FieldDef& f_src,
                                        const FieldDef& f_dst)
{
    BOND_THROW(SchemaValidateException,
        "Schemas are incompatible; required modifier removed: "
        << s_src.metadata.name << "::" << f_src.metadata.name << ", "
        << s_dst.metadata.name << "::" << f_dst.metadata.name);
}


BOND_NORETURN
inline void FieldTypeIncompatibleException(const StructDef& s_src,
                                           const StructDef& s_dst,
                                           const FieldDef& f_src,
                                           const FieldDef& f_dst)
{
    BOND_THROW(SchemaValidateException,
        "Schemas are incompatible; field types incompatible: "
        << s_src.metadata.name << "::" << f_src.metadata.name << ", "
        << s_dst.metadata.name << "::" << f_dst.metadata.name);
}


BOND_NORETURN inline void UnknownSchemaDefException(uint16_t id)
{
    BOND_THROW(SchemaValidateException,
        "Failed to validate schema compatibility; "
        "SchemaDef contains unknown field: " << id);
}

} // namespace bond
