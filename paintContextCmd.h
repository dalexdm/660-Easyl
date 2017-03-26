#pragma once
#include <maya\MPxContextCommand.h>
#include "paintContext.h"

class paintContextCmd : public MPxContextCommand
{
public:
	paintContextCmd();
	virtual MPxContext*	makeObj();
	static	void*		creator();
	//temporary, until the tool menu mentioned below is implemented
	static  void*		creator2();
	bool				isFeather = false;

	// Will eventually be used for tool menu *paired with commented methods below*
	virtual	MStatus		doEditFlags();
	virtual MStatus		doQueryFlags();
	virtual MStatus		appendSyntax();
protected:
	paintContext*		fPaintContext;
};