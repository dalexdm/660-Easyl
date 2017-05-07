#ifndef CreateEasyLNode_H_
#define CreateEasyLNode_H_

#include <maya/MPxNode.h>

class EasyLNode : public MPxNode
{
public:
					EasyLNode() {};
	virtual 		~EasyLNode() {};
	virtual MStatus compute(const MPlug& plug, MDataBlock& data);
	static  void*	creator();
	static  MStatus initialize();

	static MObject	spline;
	static MObject color;
	static MObject thick;
	static MObject brush;
	static MObject	level;
	static MObject	transparency;   
	static MTypeId id;
	static MObject spacing;    
	static int numOfSplinesCreated;
};
#endif