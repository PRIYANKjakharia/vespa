// Copyright Verizon Media. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package ai.vespa.feed.client;

import org.junit.jupiter.api.Test;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.util.Set;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ConcurrentSkipListSet;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import java.util.stream.IntStream;

import static java.nio.charset.StandardCharsets.UTF_8;
import static java.util.stream.Collectors.joining;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNull;
import static org.junit.jupiter.api.Assertions.assertTrue;

class JsonFeederTest {

    @Test
    void test() throws IOException {
        int docs = 1 << 14;
        String json = "[\n" +

                      IntStream.range(0, docs).mapToObj(i ->
                                                                "  {\n" +
                                                                "    \"id\": \"id:ns:type::abc" + i + "\",\n" +
                                                                "    \"fields\": {\n" +
                                                                "      \"lul\":\"lal\"\n" +
                                                                "    }\n" +
                                                                "  },\n"
                      ).collect(joining()) +

                      "  {\n" +
                      "    \"id\": \"id:ns:type::abc" + docs + "\",\n" +
                      "    \"fields\": {\n" +
                      "      \"lul\":\"lal\"\n" +
                      "    }\n" +
                      "  }\n" +
                      "]";
        ByteArrayInputStream in = new ByteArrayInputStream(json.getBytes(UTF_8));
        Set<String> ids = new ConcurrentSkipListSet<>();
        AtomicInteger resultsReceived = new AtomicInteger();
        AtomicBoolean completedSuccessfully = new AtomicBoolean();
        AtomicReference<Throwable> exceptionThrow = new AtomicReference<>();
        long startNanos = System.nanoTime();
        JsonFeeder.builder(new FeedClient() {

            @Override
            public CompletableFuture<Result> put(DocumentId documentId, String documentJson, OperationParameters params) {
                ids.add(documentId.userSpecific());
                return createSuccessResult(documentId);
            }

            @Override
            public CompletableFuture<Result> update(DocumentId documentId, String updateJson, OperationParameters params) {
                return createSuccessResult(documentId);
            }

            @Override
            public CompletableFuture<Result> remove(DocumentId documentId, OperationParameters params) {
                return createSuccessResult(documentId);
            }

            @Override
            public void close(boolean graceful) { }

            private CompletableFuture<Result> createSuccessResult(DocumentId documentId) {
                return CompletableFuture.completedFuture(new Result(Result.Type.success, documentId, "success", null));
            }

        }).build().feedMany(in, 1 << 7, new JsonFeeder.ResultCallback() {
            @Override public void onNextResult(Result result, Throwable error) { resultsReceived.incrementAndGet(); }
            @Override public void onError(Throwable error) { exceptionThrow.set(error); }
            @Override public void onComplete() { completedSuccessfully.set(true); }
        }).join(); // TODO: hangs when buffer is smaller than largest document
        System.err.println((json.length() / 1048576.0) + " MB in " + (System.nanoTime() - startNanos) * 1e-9 + " seconds");
        assertEquals(docs + 1, ids.size());
        assertEquals(docs + 1, resultsReceived.get());
        assertTrue(completedSuccessfully.get());
        assertNull(exceptionThrow.get());
    }

}
