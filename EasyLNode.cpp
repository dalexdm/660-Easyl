#include "EasyLNode.h"

#include <maya/MPxNode.h>
#include <maya\MFnNumericAttribute.h>
#include <maya/MFnUnitAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnNurbsCurveData.h>

#include <maya/MTime.h>
#include <maya/MFnMeshData.h>
#include <maya/MFnMesh.h>
#include <maya\MGlobal.h>

MObject EasyLNode::spline;
MObject EasyLNode::color;
MObject EasyLNode::thick;
MObject EasyLNode::brush;
MObject EasyLNode::level;
MObject EasyLNode::transparency;
MTypeId EasyLNode::id( 0x80000 );
MObject EasyLNode::spacing; 


int EasyLNode::numOfSplinesCreated = 0;

#define McheckErr(stat,msg)			\
	if ( MS::kSuccess != stat ) {	\
		cerr << msg;				\
		return MS::kFailure;		\
	}   

void* EasyLNode::creator()
{
	return new EasyLNode;
}

MStatus EasyLNode::initialize()
{
	MFnNumericAttribute numAttr;
	MFnUnitAttribute unitAttr;
	MFnTypedAttribute typedAttr;

	MStatus returnStatus;    
	// create attribute for the types     
	EasyLNode::spline = typedAttr.create( "spline", "spline", MFnNurbsCurveData::kNurbsCurve, &returnStatus );
	typedAttr.setWritable(true);
	typedAttr.setReadable(true);
	McheckErr(returnStatus, "ERROR creating EasyLNode spline attribute\n");
	// set color
	EasyLNode::color = numAttr.createColor("color", "color",  &returnStatus );
	McheckErr(returnStatus, "ERROR creating EasyLNode color attribute\n");
	//spacing    
	EasyLNode::spacing = numAttr.create("spacing", "spacing", MFnNumericData::kDouble, 0.0, &returnStatus );
	McheckErr(returnStatus, "ERROR creating EasyLNode spacing attribute\n");
	//thickness
	EasyLNode::thick = numAttr.create( "thick", "thick", MFnNumericData::kDouble, 0.0, &returnStatus ); 
	McheckErr(returnStatus, "ERROR creating EasyLNode thick attribute\n");
	//brush type
	EasyLNode::brush = numAttr.create( "brush", "brush", MFnNumericData::kInt, 0, &returnStatus ); 
	McheckErr(returnStatus, "ERROR creating EasyLNode thick attribute\n");
	EasyLNode::level = numAttr.create( "level", "level", MFnNumericData::kDouble, 0.0, &returnStatus );  
	McheckErr(returnStatus, "ERROR creating EasyLNode level attribute\n");
	EasyLNode::transparency = numAttr.create( "transparency", "transparency", MFnNumericData::kDouble, 0.0, &returnStatus );
	McheckErr(returnStatus, "ERROR creating EasyLNode transparency attribute\n");
	typedAttr.setStorable(false);

	//Add other attributes    
	returnStatus = addAttribute(EasyLNode::spline);
	McheckErr(returnStatus, "ERROR adding spline attribute\n");
	returnStatus = addAttribute(EasyLNode::color);
	McheckErr(returnStatus, "ERROR adding color attribute\n");
	returnStatus = addAttribute(EasyLNode::spacing);
	McheckErr(returnStatus, "ERROR adding spacing attribute\n");
	//added 
	returnStatus = addAttribute(EasyLNode::thick);
	McheckErr(returnStatus, "ERROR adding thick attribute\n");
	returnStatus = addAttribute(EasyLNode::brush);
	McheckErr(returnStatus, "ERROR adding brush attribute\n");
	returnStatus = addAttribute(EasyLNode::level);
	McheckErr(returnStatus, "ERROR adding level attribute\n");
	returnStatus = addAttribute(EasyLNode::transparency);
	McheckErr(returnStatus, "ERROR adding transparency attribute\n");
	return MS::kSuccess;    
}

MStatus EasyLNode::compute(const MPlug& plug, MDataBlock& data){
	MStatus returnStatus;
	MGlobal::displayInfo(MString("reached"));
	return MS::kSuccess;
}