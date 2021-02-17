// Copyright Verizon Media. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

package com.yahoo.vespa.model.container.http;

import com.yahoo.component.ComponentSpecification;
import com.yahoo.component.chain.dependencies.Dependencies;
import com.yahoo.component.chain.model.ChainedComponentModel;
import com.yahoo.config.model.api.ContainerEndpoint;
import com.yahoo.config.provision.ApplicationId;
import com.yahoo.container.bundle.BundleInstantiationSpecification;
import com.yahoo.jdisc.http.filter.security.rule.RuleBasedFilterConfig;

import java.util.List;
import java.util.Set;
import java.util.stream.Collectors;

import static com.yahoo.jdisc.http.filter.security.rule.RuleBasedFilterConfig.DefaultRule.Action.Enum.ALLOW;
import static com.yahoo.jdisc.http.filter.security.rule.RuleBasedFilterConfig.Rule.Action.Enum.BLOCK;
import static com.yahoo.jdisc.http.filter.security.rule.RuleBasedFilterConfig.Rule.Methods.Enum.DELETE;
import static com.yahoo.jdisc.http.filter.security.rule.RuleBasedFilterConfig.Rule.Methods.Enum.POST;
import static com.yahoo.jdisc.http.filter.security.rule.RuleBasedFilterConfig.Rule.Methods.Enum.PUT;

public class BlockFeedGlobalEndpointsFilterConfigProducer extends Filter implements RuleBasedFilterConfig.Producer {

    private final Set<ContainerEndpoint> endpoints;
    private final boolean dryRun;

    public BlockFeedGlobalEndpointsFilterConfigProducer(Set<ContainerEndpoint> endpoints, boolean dryRun) {
        super(createFilterComponentModel());
        this.endpoints = Set.copyOf(endpoints);
        this.dryRun = dryRun;
    }

    @Override
    public void getConfig(RuleBasedFilterConfig.Builder builder) {
        Set<String> hostNames = endpoints.stream()
                .flatMap(e -> e.names().stream())
                .collect(Collectors.toSet());
        RuleBasedFilterConfig.Rule.Builder rule = new RuleBasedFilterConfig.Rule.Builder()
                .hostNames(hostNames)
                .pathExpressions("/feed")
                .pathExpressions("/reserved-for-internal-use/feedapi")
                .methods(List.of(PUT, POST, DELETE))
                .action(BLOCK)
                .name("block-feed-global-endpoints")
                .blockResponseMessage("Feed to global endpoints are not allowed")
                .blockResponseCode(404);
        builder.rule(rule);
        builder.dryrun(dryRun);
        builder.defaultRule.action(ALLOW);
    }

    private static ChainedComponentModel createFilterComponentModel() {
        return new ChainedComponentModel(
                new BundleInstantiationSpecification(
                        new ComponentSpecification("com.yahoo.jdisc.http.filter.security.rule.RuleBasedRequestFilter"),
                        null,
                        new ComponentSpecification("jdisc-security-filters")),
                Dependencies.emptyDependencies());
    }
}
