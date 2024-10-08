// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include "featureexecutor.h"
#include <vespa/vespalib/util/classname.h>

namespace search::fef {

FeatureExecutor::FeatureExecutor() = default;


std::string
FeatureExecutor::getClassName() const
{
    return vespalib::getClassName(*this);
}

bool
FeatureExecutor::isPure()
{
    return false;
}

void
FeatureExecutor::handle_bind_inputs(std::span<const LazyValue>)
{
}

void
FeatureExecutor::handle_bind_outputs(std::span<NumberOrObject>)
{
}

void
FeatureExecutor::handle_bind_match_data(const MatchData &)
{
}

void
FeatureExecutor::bind_inputs(std::span<const LazyValue> inputs)
{
    _inputs.bind(inputs);
    handle_bind_inputs(inputs);
}

void
FeatureExecutor::bind_outputs(std::span<NumberOrObject> outputs)
{
    _outputs.bind(outputs);
    handle_bind_outputs(outputs);
}

void
FeatureExecutor::bind_match_data(const MatchData &md)
{
    handle_bind_match_data(md);
}

}
