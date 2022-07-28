// Copyright Yahoo. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.hosted.node.admin.maintenance.coredump;

import com.yahoo.vespa.hosted.node.admin.container.ContainerOperations;
import com.yahoo.vespa.hosted.node.admin.nodeagent.NodeAgentContext;
import com.yahoo.vespa.hosted.node.admin.nodeagent.NodeAgentContextImpl;
import com.yahoo.vespa.hosted.node.admin.task.util.fs.ContainerPath;
import com.yahoo.vespa.hosted.node.admin.task.util.process.CommandResult;
import com.yahoo.vespa.test.file.TestFileSystem;
import org.junit.jupiter.api.Test;

import java.util.List;
import java.util.Map;

import static com.yahoo.vespa.hosted.node.admin.maintenance.coredump.CoreCollector.GDB_PATH_RHEL8;
import static com.yahoo.vespa.hosted.node.admin.maintenance.coredump.CoreCollector.JAVA_HEAP_DUMP_METADATA;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.fail;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

/**
 * @author freva
 */
public class CoreCollectorTest {
    private final String JDK_PATH = "/path/to/jdk/java";
    private final ContainerOperations docker = mock(ContainerOperations.class);
    private final CoreCollector coreCollector = new CoreCollector(docker);
    private final NodeAgentContext context = NodeAgentContextImpl.builder("container-123.domain.tld")
            .fileSystem(TestFileSystem.create()).build();

    private final ContainerPath TEST_CORE_PATH = context.paths().of("/tmp/core.1234");
    private final String TEST_BIN_PATH = "/usr/bin/program";
    private final List<String> GDB_BACKTRACE = List.of("[New Thread 2703]",
            "Core was generated by `/usr/bin/program\'.", "Program terminated with signal 11, Segmentation fault.",
            "#0  0x00000000004004d8 in main (argv=...) at main.c:4", "4\t    printf(argv[3]);",
            "#0  0x00000000004004d8 in main (argv=...) at main.c:4");

    @Test
    void extractsBinaryPathTest() {
        final String[] cmd = {"file", TEST_CORE_PATH.pathInContainer()};

        mockExec(cmd,
                "/tmp/core.1234: ELF 64-bit LSB core file x86-64, version 1 (SYSV), SVR4-style, from " +
                        "'/usr/bin/program'");
        assertEquals(TEST_BIN_PATH, coreCollector.readBinPath(context, TEST_CORE_PATH));

        mockExec(cmd,
                "/tmp/core.1234: ELF 64-bit LSB core file x86-64, version 1 (SYSV), SVR4-style, from " +
                        "'/usr/bin/program --foo --bar baz'");
        assertEquals(TEST_BIN_PATH, coreCollector.readBinPath(context, TEST_CORE_PATH));

        mockExec(cmd,
                "/tmp/core.1234: ELF 64-bit LSB core file x86-64, version 1 (SYSV), SVR4-style, from " +
                        "'/usr/bin/program'");
        assertEquals(TEST_BIN_PATH, coreCollector.readBinPath(context, TEST_CORE_PATH));

        mockExec(cmd,
                "/tmp/core.1234: ELF 64-bit LSB core file x86-64, version 1 (SYSV), SVR4-style, " +
                        "from 'program', real uid: 0, effective uid: 0, real gid: 0, effective gid: 0, " +
                        "execfn: '/usr/bin/program', platform: 'x86_64");
        assertEquals(TEST_BIN_PATH, coreCollector.readBinPath(context, TEST_CORE_PATH));

        String fallbackResponse = "/response/from/fallback";
        mockExec(new String[]{"/bin/sh", "-c", GDB_PATH_RHEL8 + " -n -batch -core /tmp/core.1234 | grep '^Core was generated by'"},
                "Core was generated by `/response/from/fallback'.");
        mockExec(cmd,
                "/tmp/core.1234: ELF 64-bit LSB core file x86-64, version 1 (SYSV), SVR4-style");
        assertEquals(fallbackResponse, coreCollector.readBinPath(context, TEST_CORE_PATH));

        mockExec(cmd, "", "Error code 1234");
        assertEquals(fallbackResponse, coreCollector.readBinPath(context, TEST_CORE_PATH));
    }

    @Test
    void extractsBinaryPathUsingGdbTest() {
        final String[] cmd = new String[]{"/bin/sh", "-c",
                GDB_PATH_RHEL8 + " -n -batch -core /tmp/core.1234 | grep '^Core was generated by'"};

        mockExec(cmd, "Core was generated by `/usr/bin/program-from-gdb --identity foo/search/cluster.content_'.");
        assertEquals("/usr/bin/program-from-gdb", coreCollector.readBinPathFallback(context, TEST_CORE_PATH));

        mockExec(cmd, "", "Error 123");
        try {
            coreCollector.readBinPathFallback(context, TEST_CORE_PATH);
            fail("Expected not to be able to get bin path");
        } catch (RuntimeException e) {
            assertEquals("Failed to extract binary path from GDB, result: exit status 1, output 'Error 123', command: " +
                    "[/bin/sh, -c, /opt/rh/gcc-toolset-11/root/bin/gdb -n -batch -core /tmp/core.1234 | grep '^Core was generated by']", e.getMessage());
        }
    }

    @Test
    void extractsBacktraceUsingGdb() {
        mockExec(new String[]{GDB_PATH_RHEL8, "-n", "-ex", "set print frame-arguments none",
                        "-ex", "bt", "-batch", "/usr/bin/program", "/tmp/core.1234"},
                String.join("\n", GDB_BACKTRACE));
        assertEquals(GDB_BACKTRACE, coreCollector.readBacktrace(context, TEST_CORE_PATH, TEST_BIN_PATH, false));

        mockExec(new String[]{GDB_PATH_RHEL8, "-n", "-ex", "set print frame-arguments none",
                        "-ex", "bt", "-batch", "/usr/bin/program", "/tmp/core.1234"},
                "", "Failure");
        try {
            coreCollector.readBacktrace(context, TEST_CORE_PATH, TEST_BIN_PATH, false);
            fail("Expected not to be able to read backtrace");
        } catch (RuntimeException e) {
            assertEquals("Failed to read backtrace exit status 1, output 'Failure', Command: " +
                    "[" + GDB_PATH_RHEL8 + ", -n, -ex, set print frame-arguments none, -ex, bt, -batch, " +
                    "/usr/bin/program, /tmp/core.1234]", e.getMessage());
        }
    }

    @Test
    void extractsBacktraceFromAllThreadsUsingGdb() {
        mockExec(new String[]{GDB_PATH_RHEL8, "-n",
                        "-ex", "set print frame-arguments none",
                        "-ex", "thread apply all bt", "-batch",
                        "/usr/bin/program", "/tmp/core.1234"},
                String.join("\n", GDB_BACKTRACE));
        assertEquals(GDB_BACKTRACE, coreCollector.readBacktrace(context, TEST_CORE_PATH, TEST_BIN_PATH, true));
    }

    @Test
    void collectsDataTest() {
        mockExec(new String[]{"file", TEST_CORE_PATH.pathInContainer()},
                "/tmp/core.1234: ELF 64-bit LSB core file x86-64, version 1 (SYSV), SVR4-style, from " +
                        "'/usr/bin/program'");
        mockExec(new String[]{GDB_PATH_RHEL8, "-n", "-ex", "set print frame-arguments none",
                        "-ex", "bt", "-batch", "/usr/bin/program", "/tmp/core.1234"},
                String.join("\n", GDB_BACKTRACE));
        mockExec(new String[]{GDB_PATH_RHEL8, "-n", "-ex", "set print frame-arguments none",
                        "-ex", "thread apply all bt", "-batch",
                        "/usr/bin/program", "/tmp/core.1234"},
                String.join("\n", GDB_BACKTRACE));

        Map<String, Object> expectedData = Map.of(
                "bin_path", TEST_BIN_PATH,
                "backtrace", GDB_BACKTRACE,
                "backtrace_all_threads", GDB_BACKTRACE);
        assertEquals(expectedData, coreCollector.collect(context, TEST_CORE_PATH));
    }

    @Test
    void collectsPartialIfBacktraceFailsTest() {
        mockExec(new String[]{"file", TEST_CORE_PATH.pathInContainer()},
                "/tmp/core.1234: ELF 64-bit LSB core file x86-64, version 1 (SYSV), SVR4-style, from " +
                        "'/usr/bin/program'");
        mockExec(new String[]{GDB_PATH_RHEL8 + " -n -ex set print frame-arguments none -ex bt -batch /usr/bin/program /tmp/core.1234"},
                "", "Failure");

        Map<String, Object> expectedData = Map.of("bin_path", TEST_BIN_PATH);
        assertEquals(expectedData, coreCollector.collect(context, TEST_CORE_PATH));
    }

    @Test
    void reportsJstackInsteadOfGdbForJdkCores() {
        mockExec(new String[]{"file", TEST_CORE_PATH.pathInContainer()},
                "dump.core.5954: ELF 64-bit LSB core file x86-64, version 1 (SYSV), too many program header sections (33172)");

        mockExec(new String[]{"/bin/sh", "-c", GDB_PATH_RHEL8 + " -n -batch -core /tmp/core.1234 | grep '^Core was generated by'"},
                "Core was generated by `" + JDK_PATH + " -Dconfig.id=default/container.11 -XX:+Pre'.");

        String jstack = "jstack11";
        mockExec(new String[]{"jhsdb", "jstack", "--exe", JDK_PATH, "--core", "/tmp/core.1234"},
                jstack);

        Map<String, Object> expectedData = Map.of(
                "bin_path", JDK_PATH,
                "backtrace_all_threads", List.of(jstack));
        assertEquals(expectedData, coreCollector.collect(context, TEST_CORE_PATH));
    }

    @Test
    void metadata_for_java_heap_dump() {
        assertEquals(JAVA_HEAP_DUMP_METADATA, coreCollector.collect(context, context.paths().of("/dump_java_pid123.hprof")));
    }

    private void mockExec(String[] cmd, String output) {
        mockExec(cmd, output, "");
    }

    private void mockExec(String[] cmd, String output, String error) {
        mockExec(context, cmd, output, error);
    }

    private void mockExec(NodeAgentContext context, String[] cmd, String output, String error) {
        when(docker.executeCommandInContainer(context, context.users().root(), cmd))
                .thenReturn(new CommandResult(null, error.isEmpty() ? 0 : 1, error.isEmpty() ? output : error));
    }
}
