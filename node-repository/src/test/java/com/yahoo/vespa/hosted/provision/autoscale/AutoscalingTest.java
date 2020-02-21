// Copyright 2019 Oath Inc. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.hosted.provision.autoscale;

import com.yahoo.config.provision.ApplicationId;
import com.yahoo.config.provision.CloudName;
import com.yahoo.config.provision.ClusterSpec;
import com.yahoo.config.provision.Environment;
import com.yahoo.config.provision.Flavor;
import com.yahoo.config.provision.NodeResources;
import com.yahoo.config.provision.RegionName;
import com.yahoo.config.provision.SystemName;
import com.yahoo.config.provision.Zone;
import org.junit.Test;

import java.util.ArrayList;
import java.util.List;

import static org.junit.Assert.assertTrue;

/**
 * @author bratseth
 */
public class AutoscalingTest {

    @Test
    public void testAutoscalingSingleGroup() {
        NodeResources resources = new NodeResources(3, 100, 100, 1);
        AutoscalingTester tester = new AutoscalingTester(resources);

        ApplicationId application1 = tester.applicationId("application1");
        ClusterSpec cluster1 = tester.clusterSpec(ClusterSpec.Type.container, "cluster1");

        // deploy
        tester.deploy(application1, cluster1, 5, 1, resources);

        assertTrue("No measurements -> No change", tester.autoscale(application1, cluster1).isEmpty());

        tester.addMeasurements(Resource.cpu, 0.25f, 60, application1);
        assertTrue("Too few measurements -> No change", tester.autoscale(application1, cluster1).isEmpty());

        tester.addMeasurements(Resource.cpu,  0.25f, 60, application1);
        ClusterResources scaledResources = tester.assertResources("Scaling up since resource usage is too high",
                                                                 10, 1, 1.7,  44.4, 44.4,
                                                                  tester.autoscale(application1, cluster1));

        tester.deploy(application1, cluster1, scaledResources);
        assertTrue("Cluster in flux -> No further change", tester.autoscale(application1, cluster1).isEmpty());

        tester.deactivateRetired(application1, cluster1, scaledResources);
        tester.addMeasurements(Resource.cpu, 0.8f, 3, application1);
        assertTrue("Load change is large, but insufficient measurements for new config -> No change",
                   tester.autoscale(application1, cluster1).isEmpty());

        tester.addMeasurements(Resource.cpu,  0.19f, 100, application1);
        assertTrue("Load change is small -> No change", tester.autoscale(application1, cluster1).isEmpty());

        tester.addMeasurements(Resource.cpu,  0.1f, 120, application1);
        tester.assertResources("Scaling down since resource usage has gone down significantly",
                               10, 1, 1.2, 44.4, 44.4,
                               tester.autoscale(application1, cluster1));
    }

    @Test
    public void testAutoscalingGroupSize1() {
        NodeResources resources = new NodeResources(3, 100, 100, 1);
        AutoscalingTester tester = new AutoscalingTester(resources);

        ApplicationId application1 = tester.applicationId("application1");
        ClusterSpec cluster1 = tester.clusterSpec(ClusterSpec.Type.container, "cluster1");

        // deploy
        tester.deploy(application1, cluster1, 5, 5, resources);
        tester.addMeasurements(Resource.cpu,  0.25f, 120, application1);
        tester.assertResources("Scaling up since resource usage is too high",
                               10, 10, 1.7,  44.4, 44.4,
                               tester.autoscale(application1, cluster1));
    }

    @Test
    public void testAutoscalingGroupSize3() {
        NodeResources resources = new NodeResources(3, 100, 100, 1);
        AutoscalingTester tester = new AutoscalingTester(resources);

        ApplicationId application1 = tester.applicationId("application1");
        ClusterSpec cluster1 = tester.clusterSpec(ClusterSpec.Type.container, "cluster1");

        // deploy
        tester.deploy(application1, cluster1, 6, 2, resources);
        tester.addMeasurements(Resource.cpu,  0.22f, 120, application1);
        tester.assertResources("Scaling up since resource usage is too high",
                               9, 3, 2.7,  83.3, 83.3,
                               tester.autoscale(application1, cluster1));
    }

    @Test
    public void testAutoscalingAws() {
        List<Flavor> flavors = new ArrayList<>();
        flavors.add(new Flavor("aws-xlarge", new NodeResources(6, 100, 100, 1, NodeResources.DiskSpeed.fast, NodeResources.StorageType.remote)));
        flavors.add(new Flavor("aws-large", new NodeResources(3, 100, 100, 1, NodeResources.DiskSpeed.fast, NodeResources.StorageType.remote)));
        flavors.add(new Flavor("aws-medium", new NodeResources(2, 100, 100, 1, NodeResources.DiskSpeed.fast, NodeResources.StorageType.remote)));
        flavors.add(new Flavor("aws-small", new NodeResources(1, 100, 100, 1, NodeResources.DiskSpeed.fast, NodeResources.StorageType.remote)));
        AutoscalingTester tester = new AutoscalingTester(new Zone(CloudName.from("aws"), SystemName.main,
                                                                  Environment.prod, RegionName.from("us-east")),
                                                         flavors);

        ApplicationId application1 = tester.applicationId("application1");
        ClusterSpec cluster1 = tester.clusterSpec(ClusterSpec.Type.container, "cluster1");

        // deploy
        tester.deploy(application1, cluster1, 5, 1, new NodeResources(3, 100, 100, 1));

        tester.addMeasurements(Resource.cpu, 0.25f, 120, application1);
        ClusterResources scaledResources = tester.assertResources("Scaling up since resource usage is too high",
                                                                  7, 1, 3,  100, 100,
                                                                  tester.autoscale(application1, cluster1));

        tester.deploy(application1, cluster1, scaledResources);
        tester.deactivateRetired(application1, cluster1, scaledResources);

        tester.addMeasurements(Resource.cpu, 0.05f, 1000, application1);
        tester.assertResources("Scaling down since resource usage has gone down significantly",
                               8, 1, 1, 100, 100,
                               tester.autoscale(application1, cluster1));
    }

}
