// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.schema.parser;

import java.util.ArrayList;
import java.util.List;
import java.util.Optional;

/**
 * This class holds the extracted information after parsing a "fieldset"
 * block, using simple data structures as far as possible.  Do not put
 * advanced logic here!
 * @author arnej27959
 **/
public class ParsedFieldSet extends ParsedBlock {

    private final List<String> fields = new ArrayList<>();
    private final List<String> queryCommands = new ArrayList<>();
    private ParsedMatchSettings matchInfo = null;

    public ParsedFieldSet(String name) {
        super(name, "fieldset");
    }

    public ParsedMatchSettings matchSettings() {
        if (matchInfo == null) matchInfo = new ParsedMatchSettings();
        return this.matchInfo;
    }

    List<String> getQueryCommands() { return List.copyOf(queryCommands); }
    List<String> getFieldNames() { return List.copyOf(fields); }
    Optional<ParsedMatchSettings> getMatchSettings() {
        return Optional.ofNullable(this.matchInfo);
    }

    public void addField(String field) { fields.add(field); }
    public void addQueryCommand(String command) { queryCommands.add(command); }
}
