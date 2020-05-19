// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.hosted.provision.provisioning;

import com.yahoo.component.Version;
import com.yahoo.config.provision.ApplicationId;
import com.yahoo.config.provision.ClusterSpec;
import com.yahoo.config.provision.NodeType;
import com.yahoo.config.provision.OutOfCapacityException;
import com.yahoo.lang.MutableInteger;
import com.yahoo.transaction.Mutex;
import com.yahoo.vespa.flags.FlagSource;
import com.yahoo.vespa.flags.Flags;
import com.yahoo.vespa.flags.ListFlag;
import com.yahoo.vespa.flags.custom.PreprovisionCapacity;
import com.yahoo.vespa.hosted.provision.LockedNodeList;
import com.yahoo.vespa.hosted.provision.Node;
import com.yahoo.vespa.hosted.provision.NodeRepository;
import com.yahoo.vespa.hosted.provision.node.Agent;

import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import java.util.stream.Collectors;

/**
 * Performs preparation of node activation changes for a single host group in an application.
 *
 * @author bratseth
 */
public class GroupPreparer {

    private final NodeRepository nodeRepository;
    private final Optional<HostProvisioner> hostProvisioner;
    private final ListFlag<PreprovisionCapacity> preprovisionCapacityFlag;

    public GroupPreparer(NodeRepository nodeRepository,
                         Optional<HostProvisioner> hostProvisioner,
                         FlagSource flagSource) {
        this.nodeRepository = nodeRepository;
        this.hostProvisioner = hostProvisioner;
        this.preprovisionCapacityFlag = Flags.PREPROVISION_CAPACITY.bindTo(flagSource);
    }

    /**
     * Ensure sufficient nodes are reserved or active for the given application, group and cluster
     *
     * @param application        the application we are allocating to
     * @param cluster            the cluster and group we are allocating to
     * @param requestedNodes     a specification of the requested nodes
     * @param surplusActiveNodes currently active nodes which are available to be assigned to this group.
     *                           This method will remove from this list if it finds it needs additional nodes
     * @param highestIndex       the current highest node index among all active nodes in this cluster.
     *                           This method will increase this number when it allocates new nodes to the cluster.
     * @param spareCount         The number of spare docker hosts we want when dynamically allocate docker containers
     * @return the list of nodes this cluster group will have allocated if activated
     */
    // Note: This operation may make persisted changes to the set of reserved and inactive nodes,
    // but it may not change the set of active nodes, as the active nodes must stay in sync with the
    // active config model which is changed on activate
    public List<Node> prepare(ApplicationId application, ClusterSpec cluster, NodeSpec requestedNodes,
                              List<Node> surplusActiveNodes, MutableInteger highestIndex, int spareCount, int wantedGroups) {
        boolean dynamicProvisioningEnabled = hostProvisioner.isPresent() && nodeRepository.zone().getCloud().dynamicProvisioning();
        boolean allocateFully = dynamicProvisioningEnabled && preprovisionCapacityFlag.value().isEmpty();
        try (Mutex lock = nodeRepository.lock(application)) {

            // Lock ready pool to ensure that the same nodes are not simultaneously allocated by others
            try (Mutex allocationLock = nodeRepository.lockUnallocated()) {

                // Create a prioritized set of nodes
                LockedNodeList allNodes = nodeRepository.list(allocationLock);
                NodeAllocation allocation = new NodeAllocation(allNodes, application, cluster, requestedNodes,
                                                               highestIndex,  nodeRepository);

                NodePrioritizer prioritizer = new NodePrioritizer(allNodes,
                                                                  application,
                                                                  cluster,
                                                                  requestedNodes,
                                                                  spareCount,
                                                                  wantedGroups,
                                                                  allocateFully,
                                                                  nodeRepository);
                prioritizer.addApplicationNodes();
                prioritizer.addSurplusNodes(surplusActiveNodes);
                prioritizer.addReadyNodes();
                prioritizer.addNewDockerNodes();
                allocation.offer(prioritizer.prioritize());

                if (dynamicProvisioningEnabled) {
                    Version osVersion = nodeRepository.osVersions().targetFor(NodeType.host).orElse(Version.emptyVersion);
                    List<ProvisionedHost> provisionedHosts = allocation.getFulfilledDockerDeficit()
                            .map(deficit -> hostProvisioner.get().provisionHosts(nodeRepository.database().getProvisionIndexes(deficit.getCount()),
                                                                                 deficit.getFlavor(),
                                                                                 application,
                                                                                 osVersion))
                            .orElseGet(List::of);

                    // At this point we have started provisioning of the hosts, the first priority is to make sure that
                    // the returned hosts are added to the node-repo so that they are tracked by the provision maintainers
                    List<Node> hosts = provisionedHosts.stream()
                                                       .map(ProvisionedHost::generateHost)
                                                       .collect(Collectors.toList());
                    nodeRepository.addNodes(hosts, Agent.application);

                    // Offer the nodes on the newly provisioned hosts, this should be enough to cover the deficit
                    List<PrioritizableNode> nodes = provisionedHosts.stream()
                            .map(provisionedHost -> new PrioritizableNode.Builder(provisionedHost.generateNode())
                                    .parent(provisionedHost.generateHost())
                                    .newNode(true)
                                    .build())
                            .collect(Collectors.toList());
                    allocation.offer(nodes);
                }

                if (! allocation.fulfilled() && requestedNodes.canFail())
                    throw new OutOfCapacityException((cluster.group().isPresent() ? "Out of capacity on " + cluster.group().get() :"") +
                                                     allocation.outOfCapacityDetails());

                // Carry out and return allocation
                nodeRepository.reserve(allocation.reservableNodes());
                nodeRepository.addDockerNodes(new LockedNodeList(allocation.newNodes(), allocationLock));
                List<Node> acceptedNodes = allocation.finalNodes();
                surplusActiveNodes.removeAll(acceptedNodes);
                return acceptedNodes;
            }
        }
    }

}
