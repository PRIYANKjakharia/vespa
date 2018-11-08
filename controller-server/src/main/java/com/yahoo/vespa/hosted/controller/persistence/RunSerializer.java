// Copyright 2018 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.hosted.controller.persistence;

import com.yahoo.component.Version;
import com.yahoo.config.provision.ApplicationId;
import com.yahoo.slime.ArrayTraverser;
import com.yahoo.slime.Cursor;
import com.yahoo.slime.Inspector;
import com.yahoo.slime.ObjectTraverser;
import com.yahoo.slime.Slime;
import com.yahoo.vespa.hosted.controller.api.integration.deployment.JobType;
import com.yahoo.vespa.hosted.controller.api.integration.deployment.RunId;
import com.yahoo.vespa.hosted.controller.api.integration.deployment.ApplicationVersion;
import com.yahoo.vespa.hosted.controller.api.integration.deployment.SourceRevision;
import com.yahoo.vespa.hosted.controller.deployment.Run;
import com.yahoo.vespa.hosted.controller.deployment.RunStatus;
import com.yahoo.vespa.hosted.controller.deployment.Step;
import com.yahoo.vespa.hosted.controller.deployment.Step.Status;
import com.yahoo.vespa.hosted.controller.deployment.Versions;

import java.time.Instant;
import java.util.EnumMap;
import java.util.Optional;
import java.util.SortedMap;
import java.util.TreeMap;

import static com.yahoo.vespa.hosted.controller.deployment.RunStatus.aborted;
import static com.yahoo.vespa.hosted.controller.deployment.RunStatus.deploymentFailed;
import static com.yahoo.vespa.hosted.controller.deployment.RunStatus.installationFailed;
import static com.yahoo.vespa.hosted.controller.deployment.RunStatus.outOfCapacity;
import static com.yahoo.vespa.hosted.controller.deployment.RunStatus.running;
import static com.yahoo.vespa.hosted.controller.deployment.RunStatus.success;
import static com.yahoo.vespa.hosted.controller.deployment.RunStatus.error;
import static com.yahoo.vespa.hosted.controller.deployment.RunStatus.testFailure;
import static com.yahoo.vespa.hosted.controller.deployment.Step.Status.failed;
import static com.yahoo.vespa.hosted.controller.deployment.Step.Status.succeeded;
import static com.yahoo.vespa.hosted.controller.deployment.Step.Status.unfinished;
import static com.yahoo.vespa.hosted.controller.deployment.Step.deactivateReal;
import static com.yahoo.vespa.hosted.controller.deployment.Step.deactivateTester;
import static com.yahoo.vespa.hosted.controller.deployment.Step.deployInitialReal;
import static com.yahoo.vespa.hosted.controller.deployment.Step.deployReal;
import static com.yahoo.vespa.hosted.controller.deployment.Step.deployTester;
import static com.yahoo.vespa.hosted.controller.deployment.Step.installInitialReal;
import static com.yahoo.vespa.hosted.controller.deployment.Step.installReal;
import static com.yahoo.vespa.hosted.controller.deployment.Step.installTester;
import static com.yahoo.vespa.hosted.controller.deployment.Step.report;
import static com.yahoo.vespa.hosted.controller.deployment.Step.startTests;
import static com.yahoo.vespa.hosted.controller.deployment.Step.endTests;
import static java.util.Comparator.comparing;

/**
 * Serialises and deserialises RunStatus objects for persistent storage.
 *
 * @author jonmv
 */
class RunSerializer {

    private static final String stepsField = "steps";
    private static final String applicationField = "id";
    private static final String jobTypeField = "type";
    private static final String numberField = "number";
    private static final String startField = "start";
    private static final String endField = "end";
    private static final String statusField = "status";
    private static final String versionsField = "versions";
    private static final String platformVersionField = "platform";
    private static final String repositoryField = "repository";
    private static final String branchField = "branch";
    private static final String commitField = "commit";
    private static final String buildField = "build";
    private static final String sourceField = "source";
    private static final String lastTestRecordField = "lastTestRecord";

    Run runFromSlime(Slime slime) {
        return runFromSlime(slime.get());
    }

    SortedMap<RunId, Run> runsFromSlime(Slime slime) {
        SortedMap<RunId, Run> runs = new TreeMap<>(comparing(RunId::number));
        Inspector runArray = slime.get();
        runArray.traverse((ArrayTraverser) (__, runObject) -> {
            Run run = runFromSlime(runObject);
            runs.put(run.id(), run);
        });
        return runs;
    }

    private Run runFromSlime(Inspector runObject) {
        EnumMap<Step, Status> steps = new EnumMap<>(Step.class);
        runObject.field(stepsField).traverse((ObjectTraverser) (step, status) -> {
            steps.put(stepOf(step), stepStatusOf(status.asString()));
        });
        return new Run(new RunId(ApplicationId.fromSerializedForm(runObject.field(applicationField).asString()),
                                 JobType.fromJobName(runObject.field(jobTypeField).asString()),
                                 runObject.field(numberField).asLong()),
                       steps,
                       versionsFromSlime(runObject.field(versionsField)),
                       Instant.ofEpochMilli(runObject.field(startField).asLong()),
                       Optional.of(runObject.field(endField))
                               .filter(Inspector::valid)
                               .map(end -> Instant.ofEpochMilli(end.asLong())),
                       runStatusOf(runObject.field(statusField).asString()),
                       runObject.field(lastTestRecordField).asLong());
    }

    private Versions versionsFromSlime(Inspector versionsObject) {
        Version targetPlatformVersion = Version.fromString(versionsObject.field(platformVersionField).asString());
        ApplicationVersion targetApplicationVersion = ApplicationVersion.from(new SourceRevision(versionsObject.field(repositoryField).asString(),
                                                                                                 versionsObject.field(branchField).asString(),
                                                                                                 versionsObject.field(commitField).asString()),
                                                                              versionsObject.field(buildField).asLong());
        Optional<Version> sourcePlatformVersion = versionsObject.field(sourceField).valid()
                ? Optional.of(Version.fromString(versionsObject.field(sourceField).field(platformVersionField).asString()))
                : Optional.empty();
        Optional<ApplicationVersion> sourceApplicationVersion = versionsObject.field(sourceField).valid()
                ? Optional.of(ApplicationVersion.from(new SourceRevision(versionsObject.field(sourceField).field(repositoryField).asString(),
                                                                         versionsObject.field(sourceField).field(branchField).asString(),
                                                                         versionsObject.field(sourceField).field(commitField).asString()),
                                                      versionsObject.field(sourceField).field(buildField).asLong()))
                : Optional.empty();

        return new Versions(targetPlatformVersion, targetApplicationVersion, sourcePlatformVersion, sourceApplicationVersion);
    }

    Slime toSlime(Iterable<Run> runs) {
        Slime slime = new Slime();
        Cursor runArray = slime.setArray();
        runs.forEach(run -> toSlime(run, runArray.addObject()));
        return slime;
    }

    Slime toSlime(Run run) {
        Slime slime = new Slime();
        toSlime(run, slime.setObject());
        return slime;
    }

    private void toSlime(Run run, Cursor runObject) {
        runObject.setString(applicationField, run.id().application().serializedForm());
        runObject.setString(jobTypeField, run.id().type().jobName());
        runObject.setLong(numberField, run.id().number());
        runObject.setLong(startField, run.start().toEpochMilli());
        run.end().ifPresent(end -> runObject.setLong(endField, end.toEpochMilli()));
        runObject.setString(statusField, valueOf(run.status()));
        runObject.setLong(lastTestRecordField, run.lastTestLogEntry());

        Cursor stepsObject = runObject.setObject(stepsField);
        run.steps().forEach((step, status) -> stepsObject.setString(valueOf(step), valueOf(status)));

        Cursor versionsObject = runObject.setObject(versionsField);
        toSlime(run.versions().targetPlatform(), run.versions().targetApplication(), versionsObject);
        run.versions().sourcePlatform().ifPresent(sourcePlatformVersion -> {
            toSlime(sourcePlatformVersion,
                    run.versions().sourceApplication()
                       .orElseThrow(() -> new IllegalArgumentException("Source versions must be both present or absent.")),
                    versionsObject.setObject(sourceField));
        });
    }

    private void toSlime(Version platformVersion, ApplicationVersion applicationVersion, Cursor versionsObject) {
        versionsObject.setString(platformVersionField, platformVersion.toString());
        SourceRevision targetSourceRevision = applicationVersion.source()
                                                                .orElseThrow(() -> new IllegalArgumentException("Source revision must be present in target application version."));
        versionsObject.setString(repositoryField, targetSourceRevision.repository());
        versionsObject.setString(branchField, targetSourceRevision.branch());
        versionsObject.setString(commitField, targetSourceRevision.commit());
        versionsObject.setLong(buildField, applicationVersion.buildNumber()
                                                             .orElseThrow(() -> new IllegalArgumentException("Build number must be present in target application version.")));
    }

    static String valueOf(Step step) {
        switch (step) {
            case deployInitialReal  : return "deployInitialReal";
            case installInitialReal : return "installInitialReal";
            case deployReal         : return "deployReal";
            case installReal        : return "installReal";
            case deactivateReal     : return "deactivateReal";
            case deployTester       : return "deployTester";
            case installTester      : return "installTester";
            case deactivateTester   : return "deactivateTester";
            case startTests         : return "startTests";
            case endTests           : return "endTests";
            case report             : return "report";

            default: throw new AssertionError("No value defined for '" + step + "'!");
        }
    }

    static Step stepOf(String step) {
        switch (step) {
            case "deployInitialReal"  : return deployInitialReal;
            case "installInitialReal" : return installInitialReal;
            case "deployReal"         : return deployReal;
            case "installReal"        : return installReal;
            case "deactivateReal"     : return deactivateReal;
            case "deployTester"       : return deployTester;
            case "installTester"      : return installTester;
            case "deactivateTester"   : return deactivateTester;
            case "startTests"         : return startTests;
            case "endTests"           : return endTests;
            case "report"             : return report;

            default: throw new IllegalArgumentException("No step defined by '" + step + "'!");
        }
    }

    static String valueOf(Status status) {
        switch (status) {
            case unfinished : return "unfinished";
            case failed     : return "failed";
            case succeeded  : return "succeeded";

            default: throw new AssertionError("No value defined for '" + status + "'!");
        }
    }

    static Status stepStatusOf(String status) {
        switch (status) {
            case "unfinished" : return unfinished;
            case "failed"     : return failed;
            case "succeeded"  : return succeeded;

            default: throw new IllegalArgumentException("No status defined by '" + status + "'!");
        }
    }

    static String valueOf(RunStatus status) {
        switch (status) {
            case running            : return "running";
            case outOfCapacity      : return "outOfCapacity";
            case deploymentFailed   : return "deploymentFailed";
            case installationFailed : return "installationFailed";
            case testFailure        : return "testFailure";
            case error              : return "error";
            case success            : return "success";
            case aborted            : return "aborted";

            default: throw new AssertionError("No value defined for '" + status + "'!");
        }
    }

    static RunStatus runStatusOf(String status) {
        switch (status) {
            case "running"            : return running;
            case "outOfCapacity"      : return outOfCapacity;
            case "deploymentFailed"   : return deploymentFailed;
            case "installationFailed" : return installationFailed;
            case "testFailure"        : return testFailure;
            case "error"              : return error;
            case "success"            : return success;
            case "aborted"            : return aborted;

            default: throw new IllegalArgumentException("No run status defined by '" + status + "'!");
        }
    }

}
