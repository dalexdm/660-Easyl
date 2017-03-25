#include "paintContextCmd.h"
#include <maya\MGlobal.h>

#define kStartLevelSetFlag "-sl"
#define kStartLevelSetFlagLong "-startLevel"
#define kEndingLevelSetFlag "-el"
#define kEndingLevelSetFlagLong "-endLevel"
#define kModeFlag "-m"
#define kModeFlagLong "-mode"

paintContextCmd::paintContextCmd() {}

MPxContext* paintContextCmd::makeObj()
{
	fPaintContext = new paintContext();
	fPaintContext->setStartLevel(0);
	fPaintContext->setEndLevel(0);
	return fPaintContext;
}

void* paintContextCmd::creator()
{
	return new paintContextCmd;
}

MStatus paintContextCmd::doEditFlags()
{
	MStatus status = MS::kSuccess;

	MArgParser argData = parser();

	if (argData.isFlagSet(kStartLevelSetFlag)) {
		double sLevel;
		status = argData.getFlagArgument(kStartLevelSetFlag, 0, sLevel);
		if (!status) {
			status.perror("start flag parsing failed.");
			return status;
		}
		fPaintContext->setStartLevel(sLevel);
	}

	if (argData.isFlagSet(kEndingLevelSetFlag)) {
		double eLevel;
		status = argData.getFlagArgument(kEndingLevelSetFlag, 0, eLevel);
		if (!status) {
			status.perror("end flag parsing failed.");
			return status;
		}
		fPaintContext->setEndLevel(eLevel);
	}

	if (argData.isFlagSet(kModeFlag)) {
		int newMode;
		status = argData.getFlagArgument(kModeFlag, 0, newMode);
		if (!status) {
			status.perror("mode flag parsing failed.");
			return status;
		}
		fPaintContext->setMode(newMode);
	}

	return MS::kSuccess;
}

MStatus paintContextCmd::doQueryFlags()
{
	MArgParser argData = parser();

	if (argData.isFlagSet(kStartLevelSetFlag)) {
		setResult(fPaintContext->getStartLevel());
	}
	
	if (argData.isFlagSet(kEndingLevelSetFlag)) {
		setResult(fPaintContext->getEndLevel());
	}

	if (argData.isFlagSet(kModeFlag)) {
		setResult(fPaintContext->getMode());
	}

	return MS::kSuccess;
}

MStatus paintContextCmd::appendSyntax()
{
	MSyntax mySyntax = syntax();

	if (MS::kSuccess != mySyntax.addFlag(kStartLevelSetFlag, kStartLevelSetFlagLong,
		MSyntax::kDouble)) {
		MGlobal::displayInfo("Start Flag Init Problem");
		return MS::kFailure;
	}
	if (MS::kSuccess != mySyntax.addFlag(kEndingLevelSetFlag, kEndingLevelSetFlagLong,
		MSyntax::kDouble)) {
		MGlobal::displayInfo("End Flag Init problem");
		return MS::kFailure;
	}
	if (MS::kSuccess != mySyntax.addFlag(kModeFlag, kModeFlagLong,
		MSyntax::kDouble)) {
		MGlobal::displayInfo("Mode flag init problem");
		return MS::kFailure;
	}

	return MS::kSuccess;
}