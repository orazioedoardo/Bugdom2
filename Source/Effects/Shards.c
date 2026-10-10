/****************************/
/*   	SHARDS.C		    */
/* By Brian Greenstone      */
/* (c)2001 Pangea Software  */
/* (c)2023 Iliyas Jorio     */
/****************************/


/****************************/
/*    EXTERNALS             */
/****************************/

#include "game.h"


/****************************/
/*    PROTOTYPES            */
/****************************/

static void DrawShards(ObjNode*);
static void MoveShards(ObjNode*);

static void ExplodeGeometry_Recurse(MetaObjectPtr obj);
static void ExplodeVertexArray(MOVertexArrayData *data, MOMaterialObject *overrideTexture);


/****************************/
/*    CONSTANTS             */
/****************************/

#define	MAX_SHARDS		2500

typedef struct
{
	OGLVector3D				rot,rotDelta;
	OGLPoint3D				coord,coordDelta;
	float					decaySpeed,scale;
	Byte					mode;
	OGLMatrix4x4			matrix;

	OGLPoint3D				points[3];
	OGLTextureCoord			uvs[3];
	MOMaterialObject		*material;
	OGLColorRGBA			colorFilter;
	uint32_t				glow;
}ShardType;


/*********************/
/*    VARIABLES      */
/*********************/

static ShardType	gShards[MAX_SHARDS];
static Pool			*gShardPool = NULL;

static	float		gBoomForce,gShardDecaySpeed;
static	Byte		gShardMode;
static 	long		gShardDensity;
static 	OGLMatrix4x4	gWorkMatrix;
static 	ObjNode		*gShardSrcObj;

/************************* INIT SHARD SYSTEM ***************************/

void InitShardSystem(void)
{
	GAME_ASSERT(!gShardPool);
	gShardPool = Pool_New(MAX_SHARDS);

	ObjNode* driver = MakeNewDriverObject(SHARDS_SLOT, DrawShards, MoveShards);
	driver->StatusBits |= STATUS_BIT_NOLIGHTING;
	driver->StatusBits |= STATUS_BIT_DOUBLESIDED;
}

/*********************** DISPOSE SHARD SYSTEM ***********************/

void DisposeShardSystem(void)
{
	if (gShardPool)
	{
		Pool_Free(gShardPool);
		gShardPool = NULL;
	}
}


/****************** EXPLODE GEOMETRY ***********************/

void ExplodeGeometry(ObjNode *theNode, float boomForce, Byte particleMode, long particleDensity, float particleDecaySpeed)
{
MetaObjectPtr	theObject;

	gShardSrcObj	= theNode;
	gBoomForce 		= boomForce;
	gShardMode 		= particleMode;
	gShardDensity 	= particleDensity;
	gShardDecaySpeed = particleDecaySpeed;
	OGLMatrix4x4_SetIdentity(&gWorkMatrix);				// init to identity matrix


		/* SEE IF EXPLODING SKELETON OR PLAIN GEOMETRY */

	if (theNode->Genre == SKELETON_GENRE)
	{
		short	numMeshes,i,skelType;

		skelType = theNode->Type;
		numMeshes = theNode->Skeleton->skeletonDefinition->numDecomposedTriMeshes;

		UpdateSkinnedGeometry(theNode);				// be sure this skeleton's geometry is loaded into gLocalTriMeshesOfSkelType

		for (i = 0; i < numMeshes; i++)
		{
			ExplodeVertexArray(&gLocalTriMeshesOfSkelType[skelType][i], theNode->Skeleton->overrideTexture[i]);		// explode each trimesh individually
		}
	}
	else
	{
		theObject = theNode->BaseGroup;
		ExplodeGeometry_Recurse(theObject);
	}


				/* SUBRECURSE CHAINS */

	if (theNode->ChainNode)
		ExplodeGeometry(theNode->ChainNode, boomForce, particleMode, particleDensity, particleDecaySpeed);

}


/****************** EXPLODE GEOMETRY - RECURSE ***********************/

static void ExplodeGeometry_Recurse(MetaObjectPtr obj)
{
MOGroupObject		*groupObj;
MOMatrixObject 		*matObj;
OGLMatrix4x4		*transform;
OGLMatrix4x4  		stashMatrix;
int					i,numChildren;

MetaObjectHeader	*objHead = obj;
MOVertexArrayData	*vaData;

			/* VERIFY COOKIE */

	GAME_ASSERT(objHead->cookie == MO_COOKIE);


			/* HANDLE TYPE */

	switch(objHead->type)
	{
		case	MO_TYPE_VERTEXARRAY:
				vaData = &((MOVertexArrayObject *)obj)->objectData;
				ExplodeVertexArray(vaData, nil);
				break;

		case	MO_TYPE_MATERIAL:
				break;

		case	MO_TYPE_GROUP:
		 		groupObj = obj;
		  		stashMatrix = gWorkMatrix;											// push matrix

				numChildren = groupObj->objectData.numObjectsInGroup;				// get # objects in group

		  		for (i = 0; i < numChildren; i++)									// scan all objects in group
	    			ExplodeGeometry_Recurse(groupObj->objectData.groupContents[i]);	// sub-recurse this object

		  		gWorkMatrix = stashMatrix;											// pop matrix
				break;

		case	MO_TYPE_MATRIX:
				matObj = obj;
				transform = &matObj->matrix;										// point to matrix
				OGLMatrix4x4_Multiply(transform, &gWorkMatrix, &gWorkMatrix);		// multiply it in
				break;
	}
}


/********************** EXPLODE VERTEX ARRAY *******************************/

static void ExplodeVertexArray(MOVertexArrayData *data, MOMaterialObject *overrideTexture)
{
OGLPoint3D			centerPt = {0,0,0};
uint32_t				ind[3];
OGLTextureCoord		*uvPtr;
float				boomForce = gBoomForce;
OGLPoint3D			origin = { 0,0,0 };

	GAME_ASSERT(gShardPool);

	if (gShardMode & SHARD_MODE_FROMORIGIN)
	{
		origin.x = gShardSrcObj->Coord.x + (gShardSrcObj->BBox.max.x + gShardSrcObj->BBox.min.x) * .5f;		// set origin to center of object's bbox
		origin.y = gShardSrcObj->Coord.y + (gShardSrcObj->BBox.max.y + gShardSrcObj->BBox.min.y) * .5f;
		origin.z = gShardSrcObj->Coord.z + (gShardSrcObj->BBox.max.z + gShardSrcObj->BBox.min.z) * .5f;
	}

			/***************************/
			/* SCAN THRU ALL TRIANGLES */
			/***************************/

	for (int t = 0; t < data->numTriangles; t += gShardDensity)				// scan thru all triangles
	{
				/* GET FREE PARTICLE INDEX */

		int i = Pool_AllocateIndex(gShardPool);
		if (i == -1)														// see if all out
			break;
		GAME_ASSERT(i < MAX_SHARDS);


				/* DO POINTS */

		ind[0] = data->triangles[t].vertexIndices[0];						// get indecies of 3 points
		ind[1] = data->triangles[t].vertexIndices[1];
		ind[2] = data->triangles[t].vertexIndices[2];

		gShards[i].points[0] = data->points[ind[0]];						// get coords of 3 points
		gShards[i].points[1] = data->points[ind[1]];
		gShards[i].points[2] = data->points[ind[2]];

		OGLPoint3D_Transform(&gShards[i].points[0],&gWorkMatrix,&gShards[i].points[0]);							// transform points
		OGLPoint3D_Transform(&gShards[i].points[1],&gWorkMatrix,&gShards[i].points[1]);
		OGLPoint3D_Transform(&gShards[i].points[2],&gWorkMatrix,&gShards[i].points[2]);

		centerPt.x = (gShards[i].points[0].x + gShards[i].points[1].x + gShards[i].points[2].x) * 0.3333f;		// calc center of polygon
		centerPt.y = (gShards[i].points[0].y + gShards[i].points[1].y + gShards[i].points[2].y) * 0.3333f;
		centerPt.z = (gShards[i].points[0].z + gShards[i].points[1].z + gShards[i].points[2].z) * 0.3333f;

		gShards[i].points[0].x -= centerPt.x;								// offset coords to be around center
		gShards[i].points[0].y -= centerPt.y;
		gShards[i].points[0].z -= centerPt.z;
		gShards[i].points[1].x -= centerPt.x;
		gShards[i].points[1].y -= centerPt.y;
		gShards[i].points[1].z -= centerPt.z;
		gShards[i].points[2].x -= centerPt.x;
		gShards[i].points[2].y -= centerPt.y;
		gShards[i].points[2].z -= centerPt.z;


				/* DO VERTEX UV'S */

		uvPtr = data->uvs[0];
		if (uvPtr)																// see if also has UV (texture layer 0 only!)
		{
			gShards[i].uvs[0] = uvPtr[ind[0]];									// get vertex u/v's
			gShards[i].uvs[1] = uvPtr[ind[1]];
			gShards[i].uvs[2] = uvPtr[ind[2]];
		}

				/* DO MATERIAL INFO */

		if (overrideTexture)
			gShards[i].material = overrideTexture;
		else
		{
			if (data->numMaterials > 0)
				gShards[i].material = data->materials[0];							// keep material ptr
			else
				gShards[i].material = nil;
		}

		gShards[i].colorFilter = gShardSrcObj->ColorFilter;						// keep color
		gShards[i].glow = gShardSrcObj->StatusBits & STATUS_BIT_GLOW;

			/*********************/
			/* SET PHYSICS STUFF */
			/*********************/

		gShards[i].coord 	= centerPt;
		gShards[i].rot.x 	= gShards[i].rot.y = gShards[i].rot.z = 0;
		gShards[i].scale 	= 1.0;

		if (gShardMode & SHARD_MODE_FROMORIGIN)								// see if random deltas or from origin
		{
			OGLVector3D	v;

			v.x = centerPt.x - origin.x;									// calc vector from object's origin
			v.y = centerPt.y - origin.y;
			v.z = centerPt.z - origin.z;
			FastNormalizeVector(v.x, v.y, v.z, &v);

			gShards[i].coordDelta.x = v.x * boomForce;
			gShards[i].coordDelta.y = v.y * boomForce;
			gShards[i].coordDelta.z = v.z * boomForce;
		}
		else
		{
			gShards[i].coordDelta.x = RandomFloat2() * boomForce;
			gShards[i].coordDelta.y = RandomFloat2() * boomForce;
			gShards[i].coordDelta.z = RandomFloat2() * boomForce;
		}

		if (gShardMode & SHARD_MODE_UPTHRUST)
			gShards[i].coordDelta.y += 1.5f * gBoomForce;

		gShards[i].rotDelta.x 	= RandomFloat2() * (boomForce * .008f);			// random rotation deltas
		gShards[i].rotDelta.y 	= RandomFloat2() * (boomForce * .008f);
		gShards[i].rotDelta.z	= RandomFloat2() * (boomForce * .008f);

		gShards[i].decaySpeed 	= gShardDecaySpeed;
		gShards[i].mode 		= gShardMode;
	}
}


/************************** MOVE SHARDS ****************************/

static void MoveShards(ObjNode* theNode)
{
	(void) theNode;

	float	ty,y,fps,x,z;

	if (Pool_Empty(gShardPool))							// quick check if any particles at all
		return;

	fps = gFramesPerSecondFrac;

	for (int i = Pool_First(gShardPool), next; i >= 0; i = next)
	{
#if _DEBUG
		GAME_ASSERT(Pool_IsUsed(gShardPool, i));
#endif

		next = Pool_Next(gShardPool, i);					// get next index now so we can release the current one in this loop

				/* ROTATE IT */

		gShards[i].rot.x += gShards[i].rotDelta.x * fps;
		gShards[i].rot.y += gShards[i].rotDelta.y * fps;
		gShards[i].rot.z += gShards[i].rotDelta.z * fps;

					/* MOVE IT */

		if (gShards[i].mode & SHARD_MODE_HEAVYGRAVITY)
			gShards[i].coordDelta.y -= fps * 1000.0f;		// gravity
		else
			gShards[i].coordDelta.y -= fps * 300.0f;		// gravity

		x = (gShards[i].coord.x += gShards[i].coordDelta.x * fps);
		y = (gShards[i].coord.y += gShards[i].coordDelta.y * fps);
		z = (gShards[i].coord.z += gShards[i].coordDelta.z * fps);


					/* SEE IF BOUNCE */

		ty = GetTerrainY(x,z);								// get terrain height here
		if (y <= ty)
		{
			if (gShards[i].mode & SHARD_MODE_BOUNCE)
			{
				gShards[i].coord.y  = ty;
				gShards[i].coordDelta.y *= -0.5f;
				gShards[i].coordDelta.x *= 0.9f;
				gShards[i].coordDelta.z *= 0.9f;
			}
			else
				goto del;
		}

					/* SCALE IT */

		gShards[i].scale -= gShards[i].decaySpeed * fps;
		if (gShards[i].scale <= 0.0f)
		{
				/* DEACTIVATE THIS PARTICLE */
del:
			Pool_ReleaseIndex(gShardPool, i);
			continue;
		}

			/***************************/
			/* UPDATE TRANSFORM MATRIX */
			/***************************/

		OGLMatrix4x4	sm, rm, tm, qm, fm;

		OGLMatrix4x4_SetScale(&sm, gShards[i].scale,	gShards[i].scale, gShards[i].scale);
		OGLMatrix4x4_SetRotate_XYZ(&rm, gShards[i].rot.x, gShards[i].rot.y, gShards[i].rot.z);
		OGLMatrix4x4_SetTranslate(&tm, gShards[i].coord.x, gShards[i].coord.y, gShards[i].coord.z);

		OGLMatrix4x4_Multiply(&sm, &rm, &qm);
		OGLMatrix4x4_Multiply(&qm, &tm, &fm);		// NOTE!!!  We're saving to "fm" temp matrix and then copying it to gShards's matrix because this works around a compiler bug in XCode 5 where -Ofast and Link-time optimization is used!!!

		gShards[i].matrix = fm;
	}


		/* SKIP DRAW CALL IF NO SHARDS ACTIVE */

	if (Pool_Empty(gShardPool))
	{
		theNode->StatusBits |= STATUS_BIT_HIDDEN;
	}
	else
	{
		theNode->StatusBits &= ~STATUS_BIT_HIDDEN;
	}
}


/************************* SHARD LOOKS LIKE SHARD ****************************/
//
// Returns true if both shards can be drawn in the same draw call.
//

static Boolean ShardLooksLikeShard(const ShardType* a, const ShardType* b)
{
	return a->material == b->material
		&& a->glow == b->glow
		&& a->colorFilter.r == b->colorFilter.r
		&& a->colorFilter.g == b->colorFilter.g
		&& a->colorFilter.b == b->colorFilter.b
		&& a->colorFilter.a == b->colorFilter.a;
}


/************************* DRAW SHARDS ****************************/
//
// Each shard is a single triangle with its own transform. Instead of issuing one draw call per shard,
// shards that look alike are pre-transformed into a shared vertex buffer and drawn together.
// This matters a lot in the browser, where each draw call is expensive.
//

static void DrawShards(ObjNode* theNode)
{
	(void) theNode;

static OGLPoint3D		batchPoints[MAX_SHARDS * 3];
static OGLVector3D		batchNormals[MAX_SHARDS * 3];
static OGLTextureCoord	batchUVs[MAX_SHARDS * 3];
static int				pendingShards[MAX_SHARDS];

	if (Pool_Empty(gShardPool))						// quick check if any particles at all
		return;

			/* SET STATE */

//	glDisable(GL_CULL_FACE);
//	OGL_DisableLighting();

	// The shards don't have normals of their own, so they're lit with the current normal,
	// which GL transforms by each shard's matrix. GL_NORMALIZE is on, so only the rotation matters.
	OGLVector3D currentNormal;
	glGetFloatv(GL_CURRENT_NORMAL, (GLfloat*) &currentNormal);

	int numPending = 0;
	for (int i = Pool_First(gShardPool); i >= 0; i = Pool_Next(gShardPool, i))
	{
#if _DEBUG
		GAME_ASSERT(Pool_IsUsed(gShardPool, i));
#endif
		pendingShards[numPending++] = i;
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, batchPoints);
	glNormalPointer(GL_FLOAT, 0, batchNormals);
	glTexCoordPointer(2, GL_FLOAT, 0, batchUVs);

	while (numPending > 0)
	{
		const ShardType* look = &gShards[pendingShards[0]];

				/* SUBMIT MATERIAL */

		gGlobalColorFilter.r = look->colorFilter.r;
		gGlobalColorFilter.g = look->colorFilter.g;
		gGlobalColorFilter.b = look->colorFilter.b;
		gGlobalTransparency = look->colorFilter.a;

		if (look->glow)
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		else
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


		if (look->material)
			MO_DrawMaterial(look->material);

				/* TRANSFORM ALL SHARDS THAT LOOK THE SAME INTO THE BATCH */
				// (shards that don't match stay pending for a later batch)

		int numPoints = 0;
		int numStillPending = 0;

		for (int p = 0; p < numPending; p++)
		{
			const ShardType* shard = &gShards[pendingShards[p]];

			if (!ShardLooksLikeShard(shard, look))
			{
				pendingShards[numStillPending++] = pendingShards[p];
				continue;
			}

			OGLVector3D normal;
			OGLVector3D_Transform(&currentNormal, &shard->matrix, &normal);		// rotated and normalized

			for (int v = 0; v < 3; v++)
			{
				OGLPoint3D_Transform(&shard->points[v], &shard->matrix, &batchPoints[numPoints]);
				batchNormals[numPoints] = normal;
				batchUVs[numPoints] = shard->uvs[v];
				numPoints++;
			}
		}

		numPending = numStillPending;

				/* DRAW THE BATCH */

		glDrawArrays(GL_TRIANGLES, 0, numPoints);
	}

		/* CLEANUP */

	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glNormal3f(currentNormal.x, currentNormal.y, currentNormal.z);

	gGlobalColorFilter.r =
	gGlobalColorFilter.g =
	gGlobalColorFilter.b =
	gGlobalTransparency = 1;
//	glEnable(GL_CULL_FACE);
//	OGL_EnableLighting();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
