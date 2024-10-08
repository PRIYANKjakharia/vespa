// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
/**
 * @file serializable.h
 * @ingroup document
 *
 * @brief Interfaces to be used for serializing of objects.
 *
 * @author Thomas F. Gundersen, H�kon Humberset
 * @date 2004-03-15
 * @version $Id$
 */

#pragma once

#include <vespa/vespalib/util/exceptions.h>

namespace document {

class DeserializeException : public vespalib::IoException {
public:
    DeserializeException(const std::string& msg, const std::string& location = "");
    DeserializeException(const std::string& msg, const vespalib::Exception& cause,
                         const std::string& location = "");
    VESPA_DEFINE_EXCEPTION_SPINE(DeserializeException)
};

}
