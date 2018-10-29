// Copyright 2018 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.time;

import com.google.common.util.concurrent.UncheckedTimeoutException;

import java.time.Clock;
import java.time.Duration;
import java.time.Instant;
import java.util.Optional;

/**
 * A TimeBudget can be used to track the time of an ongoing operation, possibly with a timeout.
 *
 * @author hakon
 */
public class TimeBudget {
    private final Clock clock;
    private final Instant start;
    private final Optional<Duration> timeout;

    /** Returns a TimeBudget with a start time of now, and with the given timeout. */
    public static TimeBudget fromNow(Clock clock, Duration timeout) {
        return new TimeBudget(clock, clock.instant(), Optional.of(timeout));
    }

    public static TimeBudget from(Clock clock, Instant start, Optional<Duration> timeout) {
        return new TimeBudget(clock, start, timeout);
    }

    private TimeBudget(Clock clock, Instant start, Optional<Duration> timeout) {
        this.clock = clock;
        this.start = start;
        this.timeout = timeout.map(TimeBudget::makeNonNegative);
    }

    /** Returns time since start. */
    public Duration timePassed() {
        return nonNegativeBetween(start, clock.instant());
    }

    /** Returns the original timeout, if any. */
    public Optional<Duration> originalTimeout() {
        return timeout;
    }

    /** Returns the deadline, if present. */
    public Optional<Instant> deadline() {
        return timeout.map(start::plus);
    }

    /**
     * Returns the time until deadline, if there is one.
     *
     * @return time until deadline. It's toMillis() is guaranteed to be positive.
     * @throws UncheckedTimeoutException if the deadline has been reached or passed.
     */
    public Optional<Duration> timeLeftOrThrow() {
        return timeout.map(timeout -> {
            Duration passed = timePassed();
            Duration left = timeout.minus(passed);
            if (left.toMillis() <= 0) {
                throw new UncheckedTimeoutException("Time since start " + passed + " exceeds timeout " + this.timeout);
            }

            return left;
        });
    }

    private static Duration nonNegativeBetween(Instant start, Instant end) {
        return makeNonNegative(Duration.between(start, end));
    }

    private static Duration makeNonNegative(Duration duration) {
        return duration.isNegative() ? Duration.ZERO : duration;
    }
}
