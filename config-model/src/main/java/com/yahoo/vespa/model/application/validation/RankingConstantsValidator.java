// Copyright Yahoo. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.model.application.validation;

import com.yahoo.config.application.api.ApplicationFile;
import com.yahoo.config.application.api.ApplicationPackage;
import com.yahoo.config.model.application.provider.FilesApplicationPackage;
import com.yahoo.config.model.deploy.DeployState;
import com.yahoo.path.Path;
import com.yahoo.searchdefinition.DistributableResource;
import com.yahoo.searchdefinition.RankProfile;
import com.yahoo.searchdefinition.Schema;
import com.yahoo.vespa.model.VespaModel;
import com.yahoo.vespa.model.application.validation.ConstantTensorJsonValidator.InvalidConstantTensorException;

import java.io.FileNotFoundException;

/**
 * RankingConstantsValidator validates all constant tensors (ranking constants) bundled with an application package
 *
 * @author Vegard Sjonfjell
 */
public class RankingConstantsValidator extends Validator {

    private static class ExceptionMessageCollector {

        String combinedMessage;
        boolean exceptionsOccurred = false;

        ExceptionMessageCollector(String messagePrelude) {
            this.combinedMessage = messagePrelude;
        }

        public ExceptionMessageCollector add(Throwable throwable, String rcName, String rcFilename) {
            exceptionsOccurred = true;
            combinedMessage += String.format("\nRanking constant '%s' (%s): %s", rcName, rcFilename, throwable.getMessage());
            return this;
        }
    }

    static class TensorValidationException extends IllegalArgumentException {
        TensorValidationException(String message) {
            super(message);
        }
    }

    @Override
    public void validate(VespaModel model, DeployState deployState) {
        ApplicationPackage applicationPackage = deployState.getApplicationPackage();
        ExceptionMessageCollector exceptionMessageCollector = new ExceptionMessageCollector("Invalid constant tensor file(s):");

        for (Schema schema : deployState.getSchemas()) {
            for (RankProfile.Constant rc : schema.constants().values()) {
                try {
                    validateRankingConstant(rc, applicationPackage);
                } catch (InvalidConstantTensorException | FileNotFoundException ex) {
                    exceptionMessageCollector.add(ex, rc.name().toString(), rc.valuePath().get());
                }
            }
        }

        if (exceptionMessageCollector.exceptionsOccurred) {
            throw new TensorValidationException(exceptionMessageCollector.combinedMessage);
        }
    }

    private void validateRankingConstant(RankProfile.Constant rankingConstant, ApplicationPackage application) throws FileNotFoundException {
        // Only validate file backed constants:
        if (rankingConstant.valuePath().isEmpty()) return;
        if (rankingConstant.pathType().get() != DistributableResource.PathType.FILE) return;

        String constantFile = rankingConstant.valuePath().get();
        if (application.getFileReference(Path.fromString("")).getAbsolutePath().endsWith(FilesApplicationPackage.preprocessed)
            && constantFile.startsWith(FilesApplicationPackage.preprocessed)) {
            constantFile = constantFile.substring(FilesApplicationPackage.preprocessed.length());
        }

        ApplicationFile tensorApplicationFile = application.getFile(Path.fromString(constantFile));
        new ConstantTensorJsonValidator().validate(constantFile,
                                                   rankingConstant.type(),
                                                   tensorApplicationFile.createReader());
    }

}
