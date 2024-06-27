//from machine
#include <stdio.h>
#include <stdbool.h>
#include "hls_stream.h"
#include "ap_int.h"
#define BITACCURACY 9 //2^(BITACCURACY-1) > 2* MAXELEMENTS^2
#define hlsboolACCURACY 1

typedef ap_int<BITACCURACY> hlsint;
typedef ap_uint<hlsboolACCURACY> hlsbool;
#define MAXELEMENTS 9
#define NORTH 0
#define EAST 2
#define SOUTH 3
#define WEST 1
#define MAXWEIGHT 1000
#define STRONG 1
#define WEAK 0
#define MAXTOUCHES 2
#define MAX_QUEUE_SIZE 8 //4
#define MAXDEFECTEDNODES 2*MAXELEMENTS //3*MAXELEMENTS
#define MAXBLOSSOMS (MAXELEMENTS+1)/2 //MAXELEMENTS
#define MAXBLOSSOMSDIMENSION 3 //5
#define MAXCANTOUCHES 3 //5
#define MAXNODESCOUNT 2*MAXELEMENTS
#define MAXEDGES 2*(MAXELEMENTS*MAXELEMENTS)
//define that receives 2 numbers and return if the last 3 bits are the same
#define SAMEBITS(x,y) ((x&7)==(y&7))

typedef struct touchesExpansion {
	hlsint parent1;
	hlsint parent2;
	hlsint child1;
	hlsint child2;
}TouchesExpansion;

typedef struct pairDefectedNode
{
	short node1;
	short node2;
} PairDefectedNode;

typedef struct pairDNode
{
	hlsint node1;
	hlsint node2;
} PairDNode;

typedef struct nodePosition
{
	hlsint x;
	hlsint y;
} NodePosition;

typedef struct node
{
	hlsint number;
	hlsint x;
	hlsint y;
	hlsint weightGot;
	hlsbool isVirtual;
	//position in the defected nodes array
	hlsint defectedparent;
	//the edges are north, east, south, west so the dimension is 4
	hlsint edgesNear[4];
} Node;

typedef struct touch
{
	hlsint node1;
	hlsint node2;
} Touch;

typedef struct canTouchDefectedNode
{
	hlsint node;
	hlsint mychild;
	hlsint otherchild;
} CanTouchDefectedNode;

typedef struct touchDefectedNode
{
	hlsint node;
	hlsint mychild;
	hlsint otherchild;
	//0 for weak, 1 for strong
	hlsbool touchType;
} TouchDefectedNode;

typedef struct defectedNode
{
	//Can possibly add an highest parent to reduce the number of searches
	hlsint x;
	hlsint y;
	hlsint nodesCount;
	//hlsint number;
	hlsint canTouchCount;
	hlsint lastRoundModified;
	hlsint touchCount;
	// 1 for expand, -1 for reduce
	hlsint expandOrReduce;
	hlsbool hasParent;
	hlsbool isBlossom;
	hlsbool connectedToVirtual;
	hlsbool usable;
	//position in the blossoms array
	hlsint highestParent;
	//position in the blossoms array
	hlsint directParent;
	//position in the blossoms array
	hlsint myself;
	NodePosition nodes[MAXNODESCOUNT];
	TouchDefectedNode touch[MAXTOUCHES];
	CanTouchDefectedNode canTouch[MAXCANTOUCHES];// max at MAXELEMENTS*MAXELEMENTS, should be 2*MAXELEMENTS-2
} DefectedNode;

typedef	struct blossom
{
	hlsint childrenDefectedNodes[MAXBLOSSOMSDIMENSION];
	hlsint childrenDefectedNodesCount;
	//initializing the dimension to 0
	//+1 if expand, -1 if reduce
	//if 0 at any point after the initialization, then the blossom is not dismanted
	hlsint dimension;
	//position in the defected nodes array
	hlsint myself;
}Blossom;

//the graph is represented as an adjacency matrix
//the total number of nodes is n * (n-1)
//if j=0 or j=MAXELEMENTS, then the node is a virtual node
// and do not need to be expanded
// to position defected nodes do the following:
//		to find x position do n%d+1
//		to find y position do n/d
Node graph_1[MAXELEMENTS][MAXELEMENTS + 1];

//THE EDGES ARE NORTH, EAST, SOUTH, WEST
//if x=0 only east
//if x=d+1 only west
//if y=0 only east, south, west
//if y=d only north, east, west  
hlsint edgesWeight_1[MAXEDGES];

hlsint numberOfEdges_1 = 0;

//Usually only 1% to 5% of the nodes are defected
// need to add for possible blossoms
DefectedNode defectedNodes_1[MAXDEFECTEDNODES];

hlsint defectedNodesCount_1 = 0;

Blossom blossoms_1[MAXBLOSSOMS];

hlsint blossomsCount_1 = 0;

hlsint actualRound_1 = 0;

NodePosition nodeToChange_1[MAXNODESCOUNT];

hlsint nodeToChangeCount_1 = 0;

hlsbool updated_1 = false;

hlsint error = -1;
/**
 * Called when a STRONG connection is create between topBlossomFather that is a Blossom connecting to the defected node child
 * It changes all touches of child to WEAK and change accordingly all other type touches inside the Blossom
 * If due to that is created a STRONG connection to another Blossom the latter is added to the externalQueue and its child to the externalQueueChild
 * After modifing all nodes in the Blossom, if the latter is part of another blossom, it becomes the child
 * and everything continues until it reaches the point in which the child deirect parent is topBlossomFather,
 * if another blossom was added to the external queue the cycle continues
 */
void modifyOnlyMyInternalConnections(hlsint topBlossomFather, hlsint child) {
#pragma HLS INLINE off
	// Modify the touch of the child to have both outgoing connections as weak
	// Start going through the chain and modify 1 strong and 1 weak connection until it returns to the child
			// if both weak all after are correct
	if (defectedNodes_1[child].touch[0].touchType != WEAK || defectedNodes_1[child].touch[1].touchType != WEAK)
	{
		defectedNodes_1[child].touch[0].touchType = WEAK;
		defectedNodes_1[child].touch[1].touchType = WEAK;
		hlsint firstNode = defectedNodes_1[child].touch[0].node;
		hlsint secondNode = defectedNodes_1[child].touch[1].node;
		if (defectedNodes_1[firstNode].touch[0].node != child)
		{
			defectedNodes_1[firstNode].touch[0].touchType = STRONG;
			defectedNodes_1[firstNode].touch[1].touchType = WEAK;
		}
		else {
			defectedNodes_1[firstNode].touch[1].touchType = STRONG;
			defectedNodes_1[firstNode].touch[0].touchType = WEAK;
		}
		if (defectedNodes_1[secondNode].touch[0].node != child)
		{
			defectedNodes_1[secondNode].touch[0].touchType = STRONG;
			defectedNodes_1[secondNode].touch[1].touchType = WEAK;
		}
		else {
			defectedNodes_1[secondNode].touch[1].touchType = STRONG;
			defectedNodes_1[secondNode].touch[0].touchType = WEAK;
		}
	}
}
/**
 * Delete the canTouch in both node1 and node2 and reset the position in the array
 */
void deleteCanTouch(hlsint node1, hlsint node2)
{
#pragma HLS INLINE off
	defectedNodes_1[node1].canTouchCount--;
	defectedNodes_1[node2].canTouchCount--;
	hlsint i, j;
	// Find the canTouch in node1
	hlsint canTouchIndex1 = -1;
	hlsint canTouchIndex = -1;
	hlsint touches1 = defectedNodes_1[node1].canTouchCount;
	hlsint touches2 = defectedNodes_1[node2].canTouchCount;
loopfindIndex1:for (i = 0; i < MAXCANTOUCHES; i++)
{
	if (i <= touches1)
	{
		if (defectedNodes_1[node1].canTouch[i].node == node2)
		{
			canTouchIndex1 = i;
		}
	}
}

loopfindIndex2:for (j = 0; j < MAXCANTOUCHES; j++)
{
	if (j <= touches1)
	{
		if (defectedNodes_1[node2].canTouch[j].node == node1)
		{
			canTouchIndex = j;
		}
	}
}
// If canTouch is found, delete it
if (canTouchIndex1 != -1)
{
	// Update the values
	// If it's not the last element, shift the remaining elements to the left
	if (canTouchIndex1 != touches1)
	{
	loopshiftIndex1:for (i = 0; i < MAXCANTOUCHES - 1; i++)
	{
		if (i >= canTouchIndex1 && i < touches1)
		{
			defectedNodes_1[node1].canTouch[i] = defectedNodes_1[node1].canTouch[i + 1];
		}
	}
	}
}
// Also do the same for node2
// If canTouch is found, delete it
if (canTouchIndex != -1)
{
	// Update the values
	// If it's not the last element, shift the remaining elements to the left
	if (canTouchIndex != touches2)
	{
	loopshiftIndex2:for (j = 0; j < MAXCANTOUCHES - 1; j++)
	{
		//#pragma HLS UNROLL
		if (j >= canTouchIndex && j < touches2)
		{
			defectedNodes_1[node2].canTouch[j] = defectedNodes_1[node2].canTouch[j + 1];
		}
	}
	}
}
}

/**
 * Starting from one end of the chain it changes all nodes touches from WEAk to STRONG and viceversa
 * If a Strong connection is created on a Blossom it calls modifyOnlyMyInternalConnections
 */
void changeAllInChain(hlsint startNode)
{
#pragma HLS INLINE off
	//DEBUG_PRINT(outputFile, "Changing all in chain of %d\n", startNode);
	hlsint lastNode = startNode;
	hlsint currentNode = startNode;
ChangeAllLoop:do {
#pragma HLS loop_tripcount min=2 max=5 avg=3
	// Ignore the return touch of the next node that is the same as the previous
	//invert the touch type of both touches
	if (defectedNodes_1[currentNode].touchCount == MAXTOUCHES)
	{
		defectedNodes_1[currentNode].touch[0].touchType = !defectedNodes_1[currentNode].touch[0].touchType;
		defectedNodes_1[currentNode].touch[1].touchType = !defectedNodes_1[currentNode].touch[1].touchType;
	}
	else
	{
		defectedNodes_1[currentNode].touch[0].touchType = !defectedNodes_1[currentNode].touch[0].touchType;
	}
	//if one is a blossom then modify the internal connections
	if (defectedNodes_1[currentNode].isBlossom)
	{
		if (defectedNodes_1[currentNode].touch[0].touchType == STRONG)
		{
			modifyOnlyMyInternalConnections(defectedNodes_1[defectedNodes_1[currentNode].touch[0].mychild].highestParent, defectedNodes_1[currentNode].touch[0].mychild);
		}
		else if (defectedNodes_1[currentNode].touchCount == MAXTOUCHES) {
			modifyOnlyMyInternalConnections(defectedNodes_1[defectedNodes_1[currentNode].touch[1].mychild].highestParent, defectedNodes_1[currentNode].touch[1].mychild);
		}
	}
	if (defectedNodes_1[currentNode].touch[0].node != lastNode)
	{
		//DEBUG_PRINT(outputFile, "Added node %d to the queue\n", defectedNodes_1[currentNode].touch[0].node);
		lastNode = currentNode;
		currentNode = defectedNodes_1[currentNode].touch[0].node;
	}
	else if (defectedNodes_1[currentNode].touchCount == MAXTOUCHES)
	{
		//DEBUG_PRINT(outputFile, "Added node %d to the queue\n", defectedNodes_1[currentNode].touch[1].node);
		lastNode = currentNode;
		currentNode = defectedNodes_1[currentNode].touch[1].node;
	}
	else {
		lastNode = currentNode;
		currentNode = -1;
		//break;
	}
} while (currentNode != -1);
}
/**
 * Cycles until it found a node with only 1 connection-> it is the end of the chain
 */
hlsint findLastNodeInChain(hlsint startNode)
{
#pragma HLS INLINE off
	//DEBUG_PRINT(outputFile, "Finding last node in chain of %d\n", startNode);
	hlsint lastNode = startNode;
	hlsint currentNode = startNode;
findLastLoop:do {
#pragma HLS loop_tripcount min=2 max=5 avg=3
	// Ignore the return touch of the next node that is the same as the previous
	if (defectedNodes_1[currentNode].touch[0].node != lastNode)
	{
		//DEBUG_PRINT(outputFile, "Added node %d to the queue\n", defectedNodes_1[currentNode].touch[0].node);
		lastNode = currentNode;
		currentNode = defectedNodes_1[currentNode].touch[0].node;
	}
	else if (defectedNodes_1[currentNode].touchCount == MAXTOUCHES)
	{
		//DEBUG_PRINT(outputFile, "Added node %d to the queue\n", defectedNodes_1[currentNode].touch[1].node);
		lastNode = currentNode;
		currentNode = defectedNodes_1[currentNode].touch[1].node;
	}
	else {
		lastNode = currentNode;
		currentNode = -1;
		//break;
	}
} while (currentNode != -1);
//DEBUG_PRINT(outputFile, "Last node in chain is %d\n", lastNode);
return lastNode;
}
/**
 * Connects 2 nodes
 */
void connectNodes(hlsint p1, hlsint p2, hlsint child1, hlsint child2, hlsbool type)
{
	//4 CC but p2 after p1 if can || the 2
	//DEBUG_PRINT(outputFile, "Connecting nodes %d and %d with children %d and %d with type %d\n", p1, p2, child1, child2, type);
	defectedNodes_1[p1].touch[defectedNodes_1[p1].touchCount].node = p2;
	defectedNodes_1[p1].touch[defectedNodes_1[p1].touchCount].touchType = type;
	defectedNodes_1[p1].touch[defectedNodes_1[p1].touchCount].otherchild = child2;
	defectedNodes_1[p1].touch[defectedNodes_1[p1].touchCount].mychild = child1;
	defectedNodes_1[p2].touch[defectedNodes_1[p2].touchCount].node = p1;
	defectedNodes_1[p2].touch[defectedNodes_1[p2].touchCount].touchType = type;
	defectedNodes_1[p2].touch[defectedNodes_1[p2].touchCount].otherchild = child1;
	defectedNodes_1[p2].touch[defectedNodes_1[p2].touchCount].mychild = child2;
	defectedNodes_1[p1].touchCount++;
	defectedNodes_1[p2].touchCount++;
}
/**
 *
 */
void connectCanTouch(hlsint startNode) {
	//#pragma HLS allocation function instances=deleteCanTouch limit=1
	//#pragma HLS allocation function instances=connectNodes limit=1
#pragma HLS INLINE off

	hlsint lastFather[MAX_QUEUE_SIZE];
	hlsint distance[MAX_QUEUE_SIZE];
	hlsint nodes[MAX_QUEUE_SIZE];
	//#pragma HLS ARRAY_PARTITION variable= nodes type=complete
	hlsint nodesToExpand[MAX_QUEUE_SIZE];
	hlsint nodesToExpandCount = 0;
	hlsint newNodesToexpand[MAX_QUEUE_SIZE];
	hlsint newNodesToexpandCount = 0;
	hlsint nodeschildToExpand[MAX_QUEUE_SIZE];
	hlsint nodeCount = 0;
	hlsint dist = 0;
	hlsint father = 0;
	hlsbool expandable = true;
	hlsint i;
	hlsint currentNode = startNode;
	nodes[0] = startNode;
	lastFather[0] = -1;
	distance[0] = 0;
	nodeCount++;
	//set to -1 all other nodes

	currentNode = findLastNodeInChain(startNode);
	nodes[nodeCount] = currentNode;
	lastFather[nodeCount] = 0;
	distance[nodeCount] = 0;
	nodeCount++;
	for (i = 2; i < MAX_QUEUE_SIZE; i++)
	{
		//#pragma HLS UNROLL
		nodes[i] = -1;
		lastFather[i] = -1;
		distance[i] = -1;
	}
	nodesToExpand[nodesToExpandCount] = 1;
	nodesToExpandCount++;
	nodeschildToExpand[0] = nodes[1];
createListCanTouchLoop:while (expandable) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=MAX_QUEUE_SIZE avg=2
#pragma HLS PIPELINE off
	dist++;
	expandable = false;
loopNodesToExpand:for (i = 0; i < MAX_QUEUE_SIZE; i++)
{
	//#pragma HLS UNROLL factor=MAX_QUEUE_SIZE
	if (i < nodesToExpandCount)
	{
		currentNode = nodeschildToExpand[i];
		//DEBUG_PRINT(outputFile, "Processing node %d with distance %d\n", currentNode, distance[father]);
		father = nodesToExpand[i];
		if (defectedNodes_1[currentNode].canTouchCount > 0)
		{
		chekAllpossibleTouches:for (hlsint j = MAXCANTOUCHES - 1; j >= 0; j--)
		{
			//#pragma HLS UNROLL factor=MAXCANTOUCHES
			if (j < defectedNodes_1[currentNode].canTouchCount)
			{
				//#pragma HLS occurrence cycle=MAXCANTOUCHES
				hlsint nextNode = defectedNodes_1[currentNode].canTouch[j].node;
				if (defectedNodes_1[nextNode].touchCount != MAXTOUCHES)
				{
					hlsbool found = false;
					for (hlsint k = 0; k < MAX_QUEUE_SIZE; k++)
					{
						//#pragma HLS UNROLL
						if (nodes[k] == nextNode)
						{
							found = true;
						}
					}
					if (!found)
					{
						nodes[nodeCount] = nextNode;
						lastFather[nodeCount] = father;
						distance[nodeCount] = dist;
						nodeCount++;
						//DEBUG_PRINT(outputFile, "Added node %d to the queue\n", nextNode);
						hlsint afterNode = defectedNodes_1[nextNode].touch[0].node;
						nodes[nodeCount] = afterNode;
						lastFather[nodeCount] = nodeCount - 1;
						distance[nodeCount] = dist;
						nodeCount++;
						//DEBUG_PRINT(outputFile, "Added node %d to the queue\n", afterNode);
						if (defectedNodes_1[afterNode].canTouchCount > 0)
						{
							newNodesToexpand[newNodesToexpandCount] = nodeCount - 1;
							newNodesToexpandCount++;
						}
						expandable = true;
					}
				}
			}
		}
		}
	}
}
passNodes:for (hlsint m = 0; m < MAX_QUEUE_SIZE; m++)
{
	if (m < newNodesToexpandCount)
	{
		nodesToExpand[m] = newNodesToexpand[m];
		nodeschildToExpand[m] = nodes[newNodesToexpand[m]];
	}
}
nodesToExpandCount = newNodesToexpandCount;
newNodesToexpandCount = 0;
}
hlsint maxDistance = -1;
hlsint lastNode = -1;
chackmaxDIstance:for (i = MAX_QUEUE_SIZE - 1; i >= 0; i--)
{
	if (i < nodeCount && distance[i] > maxDistance)
	{
		//DEBUG_PRINT(outputFile, "Node %d has distance %d\n", nodes[i], distance[i]);
		maxDistance = distance[i];
		lastNode = i;
	}
}
hlsint nextToConnect;
connectNodesweak:while (lastNode != -1)
{
#pragma HLS LOOP_TRIPCOUNT min=2 max=MAX_QUEUE_SIZE avg=2
	//DEBUG_PRINT(outputFile, "Last Node in chain %d has distance %d\n", nodes[lastNode], distance[lastNode]);
	lastNode = lastFather[lastNode];
	//DEBUG_PRINT(outputFile, "First Node in chain %d has distance %d\n", nodes[lastNode], distance[lastNode]);
	nextToConnect = lastNode;
	lastNode = lastFather[lastNode];
	if (lastNode != -1)
	{
		//DEBUG_PRINT(outputFile, "Connecting %d to %d with weak Connection\n", nodes[lastNode], nodes[nextToConnect]);
		hlsint cantouchC = defectedNodes_1[nodes[lastNode]].canTouchCount;
	internalDeleteCanTouch:for (i = MAXCANTOUCHES - 1; i >= 0; i--)
	{
		if (i < cantouchC && defectedNodes_1[nodes[lastNode]].canTouch[i].node == nodes[nextToConnect])
		{
			if (defectedNodes_1[nodes[lastNode]].touchCount != MAXTOUCHES && defectedNodes_1[nodes[nextToConnect]].touchCount != MAXTOUCHES)
			{
				connectNodes(nodes[lastNode], nodes[nextToConnect], defectedNodes_1[nodes[lastNode]].canTouch[i].mychild, defectedNodes_1[nodes[lastNode]].canTouch[i].otherchild, WEAK);
				deleteCanTouch(nodes[lastNode], nodes[nextToConnect]);
			}
		}
	}
	}
}
}
/**
 * Delete the touch between the two nodes
 * if completely == 0 put the touch in cantouch
 */
void deleteTouch(hlsint node1, hlsint node2, hlsbool completely)
{
#pragma HLS INLINE off
	//DEBUG_PRINT(outputFile, "Deleting touch between %d and %d\n", node1, node2);
	// Find the touch in node1
	hlsint touchIndex1 = -1;
	hlsint touchIndex = -1;
	hlsint canTouchCount1 = defectedNodes_1[node1].canTouchCount;
	hlsint canTouchCount2 = defectedNodes_1[node2].canTouchCount;
	hlsint touchCount1 = defectedNodes_1[node1].touchCount - 1;
	hlsint touchCount2 = defectedNodes_1[node2].touchCount - 1;
	if (defectedNodes_1[node1].touch[0].node == node2)
		touchIndex1 = 0;
	else
		touchIndex1 = 1;
	if (defectedNodes_1[node2].touch[0].node == node1)
		touchIndex = 0;
	else
		touchIndex = 1;

	if (!completely)
	{
		defectedNodes_1[node1].canTouch[canTouchCount1].node = node2;
		defectedNodes_1[node1].canTouch[canTouchCount1].mychild = defectedNodes_1[node1].touch[touchIndex1].mychild;
		defectedNodes_1[node1].canTouch[canTouchCount1].otherchild = defectedNodes_1[node1].touch[touchIndex1].otherchild;
		defectedNodes_1[node1].canTouchCount++;
		defectedNodes_1[node2].canTouch[canTouchCount2].node = node1;
		defectedNodes_1[node2].canTouch[canTouchCount2].mychild = defectedNodes_1[node2].touch[touchIndex].mychild;
		defectedNodes_1[node2].canTouch[canTouchCount2].otherchild = defectedNodes_1[node2].touch[touchIndex].otherchild;
		defectedNodes_1[node2].canTouchCount++;
	}
	defectedNodes_1[node1].touchCount--;
	defectedNodes_1[node2].touchCount--;


	// Update the values
	//if last element then no need to move
	//else move the last element to the position of the deleted element
	// so node is in 0 and need to move 1 to 0
	if (touchIndex1 != touchCount1)
		defectedNodes_1[node1].touch[0] = defectedNodes_1[node1].touch[1];
	// Update the values
	//if last element then no need to move
	//else move the last element to the position of the deleted element
	// so node is in 0 and need to move 1 to 0
	if (touchIndex != touchCount2)
		defectedNodes_1[node2].touch[0] = defectedNodes_1[node2].touch[1];
}
/**
 * Starting from the first node of the chain find if one the other node is in the chain ,
 * if yes return the "distance" of the nodes (if it is odd a blossom will be created)
 */
int checkForLoop(hlsint node1, hlsint node2)
{
#pragma HLS INLINE off
	//DEBUG_PRINT(outputFile, "Checking for loop\n");
	// node1 is the last node of the chain
	hlsint lastNode = node1;
	hlsint currentNode = node1;
	hlsint nodesCount = 0;

	if (defectedNodes_1[node1].touchCount == 0)
	{
		//DEBUG_PRINT(outputFile, "Node %d has no touches\n", node1);
		return 0;
	}

checkLoopForBlossom:do {
#pragma HLS loop_tripcount min=2 max=6 avg=3 //max= max number of defectednodes
	nodesCount++;
	//DEBUG_PRINT(outputFile, "Node %d has touch Count %d\n", currentNode, defectedNodes_1[currentNode].touchCount);
	// if next node is the first node in the chain, then there is a loop exit the for and the while
	if (defectedNodes_1[currentNode].touch[0].node == node2 || (defectedNodes_1[currentNode].touchCount == MAXTOUCHES && defectedNodes_1[currentNode].touch[1].node == node2))
	{
		nodesCount++;
		//DEBUG_PRINT(outputFile, "Loop found between %d and %d with dimension %d\n", node1, node2, nodesCount);
		return nodesCount;
	}
	if (defectedNodes_1[currentNode].touch[0].node != lastNode)
	{
		//DEBUG_PRINT(outputFile, "Added node %d to the queue\n", defectedNodes_1[currentNode].touch[0].node);
		lastNode = currentNode;
		currentNode = defectedNodes_1[currentNode].touch[0].node;
	}
	else if (defectedNodes_1[currentNode].touchCount == MAXTOUCHES)
	{
		//DEBUG_PRINT(outputFile, "Added node %d to the queue\n", defectedNodes_1[currentNode].touch[1].node);
		lastNode = currentNode;
		currentNode = defectedNodes_1[currentNode].touch[1].node;
	}
	else {
		lastNode = currentNode;
		currentNode = -1;
		//break;
	}
} while (currentNode != -1);
//check if there is a loop
return 0;
}

//NOT USED
/**
 * check if both of the nodes, that are in the middle of their chain are in the same one
 * if yes return the distance (if it is odd a blossom will be created)
 */
int checkIntraForLoop(hlsint node1, hlsint node2)
{
#pragma HLS INLINE off
	hlsint i;
	//DEBUG_PRINT(outputFile, "Checking for Intra loop\n");
	hls::stream<hlsint> queue;
#pragma HLS stream variable=queue depth=MAX_QUEUE_SIZE

	hlsint lastNode = node1;

	queue.write(defectedNodes_1[node1].touch[0].node);
	while (!queue.empty())
	{
		hlsint currentNode = queue.read();
		for (i = 0; i < MAXTOUCHES; i++)
		{
			if (i < defectedNodes_1[currentNode].touchCount)
			{
				hlsint nextNode = defectedNodes_1[currentNode].touch[i].node;
				// Ignore the return touch of the next node that is the same as the previous
				if (nextNode != lastNode)
				{
					queue.write(nextNode);
				}
			}
		}
		lastNode = currentNode;
	}
	//DEBUG_PRINT(outputFile, "Last node in chain is %d\n", lastNode);
	queue.write(lastNode);
	hlsint nodesCount = 0;
	hlsbool foundFirst = 0;
	hlsint whichFound = -1;
	while (!queue.empty())
	{
		hlsint currentNode = queue.read();
		if (foundFirst)
			nodesCount++;
		else {
			if (currentNode == node2)
			{
				foundFirst = 1;
				whichFound = 2;
			}
			else
			{
				whichFound = 1;
			}
			nodesCount++;
		}
		//DEBUG_PRINT(outputFile, "Node in count %d is %d\n", nodesCount, currentNode);
		for (i = 0; i < MAXTOUCHES; i++)
		{
			if (i < defectedNodes_1[currentNode].touchCount)
			{
				hlsint nextNode = defectedNodes_1[currentNode].touch[i].node;
				// Ignore the return touch of the next node that is the same as the previous
				if (whichFound == 1 && nextNode == node2)
				{
					//DEBUG_PRINT(outputFile, "Loop ----------------- found between %d and %d with dimension %d\n", currentNode, nextNode, nodesCount);
					return nodesCount;
				}
				else if (whichFound == 2 && nextNode == node1)
				{
					//DEBUG_PRINT(outputFile, "Loop --------------------- found between %d and %d with dimension %d\n", currentNode, nextNode, nodesCount);
					return nodesCount;
				}
				if (nextNode != lastNode)
				{
					//DEBUG_PRINT(outputFile, "Connection type between %d and %d is %d\n", currentNode, nextNode, defectedNodes_1[currentNode].touch[i].touchType);
					//DEBUG_PRINT(outputFile, "Added node %d to the queue\n", nextNode);
					queue.write(nextNode);
				}
			}
		}
		lastNode = currentNode;
	}
	//check if there is a loop
	return 0;
}
/**
 * called after and odd result in the loop return the 2 nodes and the nodes between them
 */

void getDefectedForLoop(hlsint startNode, hlsint endNode, hlsint nodesInLoop[], hlsint* nodesInLoopCount)
{
#pragma HLS INLINE off
	//DEBUG_PRINT(outputFile, "Getting defected nodes in loop\n");
	hls::stream<hlsint> queue;
#pragma HLS stream variable=queue depth=MAX_QUEUE_SIZE

hlsint lastNode = startNode;
	queue.write(startNode);
	//DEBUG_PRINT(outputFile, "Added node %d to the queue\n", startNode);
	while (!queue.empty())
	{
		hlsint currentNode = queue.read();
		nodesInLoop[*nodesInLoopCount] = currentNode;
		(*nodesInLoopCount)++;
		for (hlsint i = 0; i < MAXTOUCHES; i++)

	{
			if (i < defectedNodes_1[currentNode].touchCount)
			{
				hlsint nextNode = defectedNodes_1[currentNode].touch[i].node;
				// if next node is the first node in the chain, then there is a loop exit the for and the while
				if (nextNode == endNode)
				{
					nodesInLoop[*nodesInLoopCount] = nextNode;
					(*nodesInLoopCount)++;
					//DEBUG_PRINT(outputFile, "Loop found between %d and %d with dimension %d\n", startNode, nextNode, (*nodesInLoopCount));
					return;
				}
				// Ignore the return touch of the next node that is the same as the previous
				if (nextNode != lastNode)
				{
					//DEBUG_PRINT(outputFile, "Connection type between %d and %d is %d\n", currentNode, nextNode, defectedNodes_1[currentNode].touch[i].touchType);
					//DEBUG_PRINT(outputFile, "Added node %d to the queue\n", nextNode);
					queue.write(nextNode);
				}
			}
		}
		lastNode = currentNode;
	}
}
/**
 * From the start of the chain delete all weak connections in the chain
 * it's important to divide all pairs when stable
 */

void deleteWeakConnections(hlsint startNode)
{
#pragma HLS INLINE off
	hls::stream<hlsint> queue;
#pragma HLS stream variable=queue depth=MAX_QUEUE_SIZE

	hlsint lastNode = startNode;
	hlsint i;
	queue.write(startNode);
	//DEBUG_PRINT(outputFile, "Deleting weak connections\n");
	// Delete the first week connections to be sure don't create a loop

		//delete all weak connections
	if (defectedNodes_1[startNode].touch[0].touchType == WEAK)
	{
		//DEBUG_PRINT(outputFile, "Deleting touch between %d and %d because WEAK\n", startNode, defectedNodes_1[startNode].touch[0].node);
		deleteTouch(startNode, defectedNodes_1[startNode].touch[0].node, false);
	}
	else if (defectedNodes_1[startNode].touchCount == MAXTOUCHES) {// one must be weak
		//DEBUG_PRINT(outputFile, "Deleting touch between %d and %d because WEAK\n", startNode, defectedNodes_1[startNode].touch[1].node);
		deleteTouch(startNode, defectedNodes_1[startNode].touch[1].node, false);
	}
	while (!queue.empty())
	{
		hlsint currentNode = queue.read();
		//DEBUG_PRINT(outputFile, "Current node is %d\n", currentNode);
		for (i = 0; i < MAXTOUCHES; i++)
		{
			if (i < defectedNodes_1[currentNode].touchCount)
			{
				hlsint nextNode = defectedNodes_1[currentNode].touch[i].node;
				// Ignore the return touch of the next node that is the same as the previous
				if (nextNode != lastNode)
				{
					if (defectedNodes_1[currentNode].touch[i].touchType == WEAK)
					{
						//DEBUG_PRINT(outputFile, "Deleting touch between %d and %d because WEAK\n", currentNode, nextNode);
						deleteTouch(currentNode, nextNode, false);
						//break; of the for
					}
					//DEBUG_PRINT(outputFile, "Added node %d to the queue\n", nextNode);
					queue.write(nextNode);
				}
			}
		}
		lastNode = currentNode;
	}
	//DEBUG_PRINT(outputFile, "End of deleting weak connections\n");
}

/**
 * Create the blossom in both defectedNodes and blossoms
 * save the elements that are part of it divided if Blossoms or not
 */
void createBlossom(hlsint nodesInBlossom[], hlsint nodesInBlossomCount)
{
#pragma HLS INLINE off
	hlsint i, j;
	//DEBUG_PRIORITY_PRINT(outputFile, "Creating blossom\n");
	// Create a new blossom
	Blossom* newBlossom = &blossoms_1[blossomsCount_1];
	blossomsCount_1++;
	//newBlossom->parent = -1;
	newBlossom->childrenDefectedNodesCount = 0;
	//newBlossom->connectedToVirtual = false;
	//newBlossom->needToBeExpanded = 0;
	newBlossom->dimension = 0;
	newBlossom->myself = defectedNodesCount_1;
	//newBlossom->number = blossomsCount_1 - 1;
	hlsint blossomNumber = blossomsCount_1 - 1;
	// Create a new defected node for the blossom
	DefectedNode* newDefectedNode = &defectedNodes_1[defectedNodesCount_1];
	defectedNodesCount_1++;
	newDefectedNode->highestParent = -1;
	newDefectedNode->directParent = -1;
	newDefectedNode->hasParent = false;
	newDefectedNode->myself = blossomsCount_1 - 1;
	newDefectedNode->isBlossom = true;
	newDefectedNode->x = -1; // Set the x and y coordinates to -1 for the blossom
	newDefectedNode->y = -1;
	newDefectedNode->connectedToVirtual = false;
	newDefectedNode->expandOrReduce = 0;
	newDefectedNode->nodesCount = 0;
	//newDefectedNode->number = defectedNodesCount_1 - 1;
	newDefectedNode->touchCount = 0;
	newDefectedNode->canTouchCount = 0;
	newDefectedNode->lastRoundModified = actualRound_1;
	newDefectedNode->usable = true;
	hlsint defectedNodeNumber = defectedNodesCount_1 - 1;
	// Link the defected nodes in the blossom to the blossom and update their parent
	for (i = 0; i < MAXBLOSSOMSDIMENSION; i++) {
		if (defectedNodes_1[nodesInBlossom[i]].isBlossom)
			error = 1;
	}
	for (i = 0; i < MAXBLOSSOMSDIMENSION; i++)
	{
#pragma HLS PIPELINE off
#pragma HLS UNROLL factor=1
		defectedNodes_1[nodesInBlossom[i]].highestParent = defectedNodeNumber;
		if (!defectedNodes_1[nodesInBlossom[i]].hasParent)
			defectedNodes_1[nodesInBlossom[i]].directParent = defectedNodeNumber;
		defectedNodes_1[nodesInBlossom[i]].hasParent = true;
		newBlossom->childrenDefectedNodes[newBlossom->childrenDefectedNodesCount] = nodesInBlossom[i];
		newBlossom->childrenDefectedNodesCount++;
		//DEBUG_PRIORITY_PRINT(outputFile, "Defected node %d \n", nodesInBlossom[i]);
		if (defectedNodes_1[nodesInBlossom[i]].connectedToVirtual)
			newDefectedNode->connectedToVirtual = true;
	}
	//print the children of the blossom

	// Set the highest parent of all the children of the blossom

	//DEBUG_PRIORITY_PRINT(outputFile, "Number of connections of the blossom %d\n", newDefectedNode->touchCount);
	//DEBUG_PRIORITY_PRINT(outputFile, "transporting connections\n");
	// Transport the touches of the internal defected nodes to the blossom
	for (i = 0; i < MAXBLOSSOMSDIMENSION; i++)
	{
#pragma HLS PIPELINE off
#pragma HLS UNROLL factor=1
		hlsint currentNode = nodesInBlossom[i];
		//DEBUG_PRIORITY_PRINT(outputFile, "Processing touches of node %d\n", currentNode);

		for (j = 0; j < MAXTOUCHES; j++)
		{
#pragma HLS PIPELINE off
#pragma HLS UNROLL factor=1
			if (j < defectedNodes_1[currentNode].touchCount)
			{
				hlsint nextNode = defectedNodes_1[currentNode].touch[j].node;
				hlsint otherChild = defectedNodes_1[currentNode].touch[j].otherchild;
				hlsint myChild = defectedNodes_1[currentNode].touch[j].mychild;
				//DEBUG_PRIORITY_PRINT(outputFile, "Processing touch between %d and %d\n", currentNode, nextNode);
				if (defectedNodes_1[nextNode].highestParent != defectedNodeNumber)//&& number < MAXTOUCHES)
				{
					//DEBUG_PRIORITY_PRINT(outputFile, "Deleting connection between %d and %d because not child of the blossom\n", currentNode, nextNode);
					int type = defectedNodes_1[currentNode].touch[j].touchType;
					//delete the touch completely
					deleteTouch(currentNode, nextNode, true);
					// Connect the two nodes with a weak connection
					//DEBUG_PRIORITY_PRINT(outputFile, " Connection number %d between %d and %d\n", number, currentNode, nextNode);
					connectNodes(defectedNodeNumber, nextNode, myChild, otherChild, type);
					j--;
				}
			}
		}
	}
}
/**
 * This method works on the Syndrome graph, based on the number and type of connections
 * of p1 and p2 (the father nodes) decides how to modify the existing ones and create
 * a new one between p1 and p2
 */
void touchesProcessing(hlsint p1, hlsint p2, hlsint child1, hlsint child2)
{
	updated_1 = true;
#pragma HLS INLINE off
	hlsint i;
	hlsint nodeToDeleteWeak;
	hlsbool deleteWeak = false;
	hlsbool strong_weak = WEAK;
	hlsbool connect = true;
	//DEBUG_PRINT(outputFile, "Processing touches between %d and %d\n", p1, p2);
	//fprintf(outputFile, "Processing touches between %d and %d, %d and %d\n", p1, p2, child1, child2);
	if (p1 == p2)
	{
		//DEBUG_PRINT(outputFile, "Nodes have the same parent\n");
		//if the parent is the same then the children are connected
		return;
	}
	//check if parents already connected or can touch
	//DEBUG_PRINT(outputFile, "Touch count of node %d is %d\n", p1, defectedNodes_1[p1].touchCount);
	//DEBUG_PRINT(outputFile, "Touch count of node %d is %d\n", p2, defectedNodes_1[p2].touchCount);
	if (defectedNodes_1[p1].touchCount > 0 && defectedNodes_1[p1].touch[0].node == p2) {

		// if the parent is the same then the children are connected
		// DEBUG_PRINT(outputFile, "Nodes are already connected\n");
		return;

	}

	if (defectedNodes_1[p1].touchCount == MAXTOUCHES && defectedNodes_1[p1].touch[1].node == p2) {

		// if the parent is the same then the children are connected
		// DEBUG_PRINT(outputFile, "Nodes are already connected\n");
		return;

	}

	for (i = 0; i < MAXCANTOUCHES; i++)
	{
		//if in can touch and one of the 2 cannot get another connection
		if (i < defectedNodes_1[p1].canTouchCount && defectedNodes_1[p1].canTouch[i].node == p2)
		{
			//if the parent is the same then the children are connected
			//DEBUG_PRINT(outputFile, "Nodes can already touch\n");
			return;
		}
	}

	if (defectedNodes_1[p1].touchCount == 0 && defectedNodes_1[p2].touchCount == 0)
	{
		//strong touch
		//connectNodes(p1, p2, child1, child2, STRONG);
		strong_weak = STRONG;
		//if one is a blossom then need to modify the internal connections
		//DEBUG_PRINT(outputFile, "First connection of both nodes\n");
	}
	else if (defectedNodes_1[p1].touchCount == 2 || defectedNodes_1[p2].touchCount == 2)
	{
		//NO IF IT CREATE A LOOP

		int loop;

		if (defectedNodes_1[p1].touchCount == 2)
{
	if (defectedNodes_1[p2].touchCount == 2) {
		//printf("NOTDONE\n");
		loop = checkIntraForLoop(p2, p1);
	}
	else
		loop = checkForLoop(p2, p1);
}
else
	loop = checkForLoop(p1, p2);

		if (loop == 0)
		{
			//weak touch		
			defectedNodes_1[p1].canTouch[defectedNodes_1[p1].canTouchCount].node = p2;
			defectedNodes_1[p2].canTouch[defectedNodes_1[p2].canTouchCount].node = p1;
			defectedNodes_1[p1].canTouch[defectedNodes_1[p1].canTouchCount].mychild = child1;
			defectedNodes_1[p2].canTouch[defectedNodes_1[p2].canTouchCount].mychild = child2;
			defectedNodes_1[p1].canTouch[defectedNodes_1[p1].canTouchCount].otherchild = child2;
			defectedNodes_1[p2].canTouch[defectedNodes_1[p2].canTouchCount].otherchild = child1;
			defectedNodes_1[p1].canTouchCount++;
			defectedNodes_1[p2].canTouchCount++;
			//DEBUG_PRINT(outputFile, "One node has already 2 connections, added to canTouch\n");
			//DEBUG_PRINT(outputFile, "Node %d has %d canTouch\n", p1, defectedNodes_1[p1].canTouchCount);
			//DEBUG_PRINT(outputFile, "Node %d has %d canTouch\n", p2, defectedNodes_1[p2].canTouchCount);
			connect = false;
		}
		else {
			if (loop % 2 == 1)
			{
				if (loop > 3)
				{
					error = 3;
				}
				//CREATE AN ARRAY IN WHICH TO PUT THE NODES THAT ARE IN THE LOOP
				hlsint nodesInLoop[MAXELEMENTS];
				hlsint nodesInLoopCount = 0;
				if (defectedNodes_1[p1].touchCount == 2)
{
				if (defectedNodes_1[p2].touchCount == 2)
				{
					error = 2;
					//printf("NOTDONE\n");
					//getDefectedFromIntraForLoop(p2, p1, nodesInLoop, &nodesInLoopCount);
				}
				else
					getDefectedForLoop(p2, p1, nodesInLoop, &nodesInLoopCount);
}
else
				getDefectedForLoop(p1, p2, nodesInLoop, &nodesInLoopCount);
				//create blossom
				/*
				if (nodesInLoopCount > 3) {
					printf("ERROR BLOSSOM NODE WITH MORE THAN 3 nodes\n");
				}*/
				if(error==-1)
					createBlossom(nodesInLoop, nodesInLoopCount);
			}
			else
			{
				connect = false;
				//DEBUG_PRINT(outputFile, "CHECK THE LOOP IS EVEN\n");
			}

		}
	}
	else {
		hlsint firstNode;
		hlsint secondNode;
		if (defectedNodes_1[p1].touchCount == 0 || defectedNodes_1[p2].touchCount == 0)
		{
			//at least one first connection
			if (defectedNodes_1[p1].touchCount == 0)
			{
				firstNode = p1;
				secondNode = p2;
			}
			else
			{
				firstNode = p2;
				secondNode = p1;
			}
			//Second node must have only one connection
			if (defectedNodes_1[secondNode].touch[0].touchType == WEAK)
			{
				// NONE -- WEAK
				// create strong connection
				//connectNodes(p1, p2, child1, child2, STRONG);
				strong_weak = STRONG;
				//if one is a blossom then need to modify the internal connections
				//TODO DELETE ALL WEAK CONNECTIONS THE STRUCTURE IS STABLE ->  PUT IN CAN TOUCH
				deleteWeak = true;
				//deleteWeakConnections(firstNode);
				nodeToDeleteWeak = firstNode;
				//DEBUG_PRINT(outputFile, "Node %d, with no connection and Node %d, with weak connection connected with Strong connection\n", firstNode, secondNode);
			}
			else
			{
				//weak touch
				//need to see the last node in the chain and see it's touch type
				hlsint lastNode = findLastNodeInChain(secondNode);
				if (defectedNodes_1[lastNode].touch[0].touchType == WEAK)
				{
					// NONE -- STRONG (weak)
					changeAllInChain(secondNode);
					//connectNodes(p1, p2, child1, child2, STRONG);
					strong_weak = STRONG;
					//if one is a blossom then need to modify the internal connections
					//TODO DELETE ALL WEAK CONNECTIONS THE STRUCTURE IS STABLE ->  PUT IN CAN TOUCH
					//deleteWeakConnections(firstNode);
					deleteWeak = true;
					nodeToDeleteWeak = firstNode;
					//DEBUG_PRINT(outputFile, "Node %d, with no connection and Node %d, with Strong(weak) connection connected with Strong connection\n", firstNode, secondNode);
				}
				else
				{
					//NONE -- STRONG (strong)
					//weak touch
					//TODO CHECK IF CANTOUCH OF LAST NODE IS NOT 0 EXTENDS TO NEXT  MUST FIND THE LONGEST EXTENSION AND THEN CREATE THE CONNECTIONS ( PROBABLY ALL WEAK )
					connectCanTouch(secondNode);
					//connectNodes(p1, p2, child1, child2, WEAK);
					//DEBUG_PRINT(outputFile, "Node %d, with no connection and Node %d, with Strong(strong) connection connected with weak connection\n", firstNode, secondNode);
				}
			}
		}
		else
		{
			//nodes connected to 1 other node
			//DEBUG_PRINT(outputFile, "Both touch one node\n");
			//NEED TO CHECK FOR LOOP THAT WILL CREATE A BLOSSOM
			int loop = checkForLoop(p1, p2);
			if (loop == 0)
			{
				//no loops
				//DEBUG_PRINT(outputFile, "No loops\n");
				if (defectedNodes_1[p1].touch[0].touchType == WEAK || defectedNodes_1[p2].touch[0].touchType == WEAK)
				{
					if (defectedNodes_1[p1].touch[0].touchType == WEAK)
					{
						firstNode = p1;
						secondNode = p2;
					}
					else
					{
						firstNode = p2;
						secondNode = p1;
					}
					if (defectedNodes_1[secondNode].touch[0].touchType == WEAK)
					{
						// WEAK -- WEAK
						// the only way they can expand is ahaving an odd number of nodes in the chain-> the last connection must be Strong
						// so after the union the chain will have an even number of nodes and the weak connections can be dismantled
						// create strong connection
						//connectNodes(p1, p2, child1, child2, STRONG);
						strong_weak = STRONG;
						//if one is a blossom then need to modify the internal connections
						//deleteWeakConnections(firstNode);
						nodeToDeleteWeak = firstNode;
						deleteWeak = true;
						//TODO DELETE ALL WEAK CONNECTIONS THE STRUCTURE IS STABLE ->  PUT IN CAN TOUCH
						//DEBUG_PRINT(outputFile, "Node %d, with weak connection and Node %d, with weak connection connected with Strong connection\n", firstNode, secondNode);
					}
					else
					{
						//DEBUG_PRINT(outputFile, "---------------------------------------------------");
						hlsint lastNode = findLastNodeInChain(secondNode);
						if (defectedNodes_1[lastNode].touch[0].touchType == WEAK)
						{
							// WEAK -- STRONG (weak)
							changeAllInChain(secondNode);
							//connectNodes(p1, p2, child1, child2, STRONG);
							strong_weak = STRONG;
							//if one is a blossom then need to modify the internal connections
							//deleteWeakConnections(lastNode);
							deleteWeak = true;
							nodeToDeleteWeak = lastNode;
							//DEBUG_PRINT(outputFile, "Node %d, with weak connection and Node %d, with strong connection (weak) connected with Strong connection\n", firstNode, secondNode);
						}
						else {
							// WEAK -- STRONG (strong)
							//weak touch
							//TODO CHECK IF CANTOUCH OF LAST NODE IS NOT 0 EXTENDS TO NEXT  MUST FIND THE LONGEST EXTENSION AND THEN CREATE THE CONNECTIONS ( PROBABLY ALL WEAK )
							//NEED FROM STRONG PART TO CHECK IF CAN EXPAND THROUGH CANTOUCH
							changeAllInChain(firstNode);
							connectCanTouch(secondNode);
							//connectNodes(p1, p2, child1, child2, WEAK);
							//DEBUG_PRINT(outputFile, "Node %d, with weak connection and Node %d, with strong connection (strong) connected with weak connection\n", firstNode, secondNode);
						}
					}
				}
				else {
					//both strong starting connection
					firstNode = p1;
					secondNode = p2;
					//check if the last node in the chain has weak connection
					hlsint lastNode1 = findLastNodeInChain(firstNode);
					hlsint lastNode2 = findLastNodeInChain(secondNode);
					if (defectedNodes_1[lastNode1].touch[0].touchType == WEAK && defectedNodes_1[lastNode2].touch[0].touchType == WEAK)
					{
						//DEBUG_PRINT(outputFile, "Here\n");
						// STRONG (weak) -- STRONG (weak)
						//change all and add strong connection
						changeAllInChain(firstNode);
						changeAllInChain(secondNode);
						//connectNodes(p1, p2, child1, child2, STRONG);
						strong_weak = STRONG;
						//if one is a blossom then need to modify the internal connections
						//deleteWeakConnections(firstNode);
						deleteWeak = true;
						nodeToDeleteWeak = firstNode;
						//DEBUG_PRINT(outputFile, "Node %d, with strong connection and Node %d, with strong connection connected with weak connection\n", firstNode, secondNode);
					}
					else {
						//create strong connection
						if (defectedNodes_1[lastNode1].touch[0].touchType == STRONG && defectedNodes_1[lastNode2].touch[0].touchType == STRONG)
						{
							// CHECK IF POSSIBLE
							//DEBUG_PRINT(outputFile, "ERRROR CANNOT HAVE 2 STABLE STRUCTURE TO EXPAND\n");
							//add to canTouch
							defectedNodes_1[p1].canTouch[defectedNodes_1[p1].canTouchCount].node = p2;
							defectedNodes_1[p2].canTouch[defectedNodes_1[p2].canTouchCount].node = p1;
							defectedNodes_1[p1].canTouch[defectedNodes_1[p1].canTouchCount].mychild = child1;
							defectedNodes_1[p2].canTouch[defectedNodes_1[p2].canTouchCount].mychild = child2;
							defectedNodes_1[p1].canTouch[defectedNodes_1[p1].canTouchCount].otherchild = child2;
							defectedNodes_1[p2].canTouch[defectedNodes_1[p2].canTouchCount].otherchild = child1;
							defectedNodes_1[p1].canTouchCount++;
							defectedNodes_1[p2].canTouchCount++;
							connect = false;
						}
						else {
							// STRONG (weak) -- STRONG (strong)
							if (defectedNodes_1[lastNode1].touch[0].touchType == STRONG)
								connectCanTouch(firstNode);
							else
								connectCanTouch(secondNode);
							//create weak connection
							//NEED FROM STRONG PART TO CHECK IF CAN EXPAND THROUGH CANTOUCH
							//connectNodes(p1, p2, child1, child2, WEAK);
							//DEBUG_PRINT(outputFile, "Node %d, with strong connection and Node %d, with strong connection connected with weak connection\n", firstNode, secondNode);
						}
					}
				}
			}
			else {
				hlsint nodesInLoop[MAXELEMENTS];
				hlsint nodesInLoopCount = 0;
				getDefectedForLoop(p2, p1, nodesInLoop, &nodesInLoopCount);
				
				if (nodesInLoopCount > 3)
				{
					error=3;
				}
				
				createBlossom(nodesInLoop, nodesInLoopCount);
			}
		}

	}

	if (connect)
		connectNodes(p1, p2, child1, child2, strong_weak);
	if (strong_weak == STRONG) {
		if (defectedNodes_1[p1].isBlossom)
			modifyOnlyMyInternalConnections(defectedNodes_1[child1].highestParent, child1);
		if (defectedNodes_1[p2].isBlossom)
			modifyOnlyMyInternalConnections(defectedNodes_1[child2].highestParent, child2);
	}
	if (deleteWeak)
		deleteWeakConnections(nodeToDeleteWeak);
}

//get the other node connected to the edge in the model graph
Node* getOtherNode(hlsint k, Node* node, hlsint d)
{
	Node* otherNode = NULL;
	//i sould not have to check the x and y
	if (k == NORTH && node->y > 0)
	{
		otherNode = &graph_1[node->y - 1][node->x];
	}
	else if (k == EAST && node->x < d)
	{
		otherNode = &graph_1[node->y][node->x + 1];
	}
	else if (k == SOUTH && node->y < d)
	{
		otherNode = &graph_1[node->y + 1][node->x];
	}
	else if (k == WEST && node->x > 0)
	{
		otherNode = &graph_1[node->y][node->x - 1];
	}
	return otherNode;
}
/**
 * delete the defined can can only of the other node
 */
void deleteOtherCanTouch(hlsint node1, hlsint node2)
{
#pragma HLS INLINE off
	hlsint i;
	if (defectedNodes_1[node2].canTouchCount == 1)
	{
		defectedNodes_1[node2].canTouchCount = 0;
		return;
	}
	// Find the canTouch in node1
	hlsint canTouchIndex = -1;
	for (i = 0; i < MAXTOUCHES; i++)
	{
		if (i < defectedNodes_1[node2].canTouchCount && defectedNodes_1[node2].canTouch[i].node == node1)
		{
			canTouchIndex = i;
			break;
		}
	}
	// If canTouch is found, delete it
	if (canTouchIndex != -1)
	{
		// Update the values
		// If it's not the last element, shift the remaining elements to the left
		if (canTouchIndex != defectedNodes_1[node2].canTouchCount - 1)
		{
			for (i = canTouchIndex; i < MAXCANTOUCHES; i++)
			{
				if (i < defectedNodes_1[node2].canTouchCount - 1)
					defectedNodes_1[node2].canTouch[i] = defectedNodes_1[node2].canTouch[i + 1];
			}
		}
		defectedNodes_1[node2].canTouchCount--;
	}
}
/**
 * if the defected node must be retracted:
 * retract the expansion in the model graph ( add MAXWEIGHT/2 to near edges),
 * if already full get the previous expansion node and retract it
 * at the end delete all can touch with other nodes-> if they expand will be recreated
 * Doesn't modify the Syndrome graph because the othe nodes will expand and nothing will change
 */
void processRetractDefectedNodes(hlsint d, hlsint defectedNode)
{
#pragma HLS INLINE off
	hlsint count = defectedNodes_1[defectedNode].nodesCount;
	NodePosition nodes[MAXNODESCOUNT];
	hlsint nCount = 0;
	hlsint j, k;
	//else continue with the new ones
	for (j = 0; j < count; j++)
	{
		//#pragma HLS UNROLL factor=1
		NodePosition pos = defectedNodes_1[defectedNode].nodes[j];
		if (pos.x != -1 && pos.y != -1)
		{
			Node* node = &graph_1[pos.y][pos.x];
			//DEBUG_PRINT(outputFile, "Retract node %d\n", node->number);
			if (node->weightGot != 0)
			{
				//DEBUG_PRINT(outputFile, "Node %d is not retracted\n", node->number);
				for (k = 0; k < 4; k++)
				{
					if (node->edgesNear[k] != -1 && edgesWeight_1[node->edgesNear[k]] != MAXWEIGHT)
					{
						//DEBUG_PRINT(outputFile, "Node %d, Edge %d, Weight %d\n", node->number, node->edgesNear[k], *edge);
						// Check if the node on the other side is has the same parent if yes don't retract
						Node* otherNode = getOtherNode(k, node, d);
						//if the other node is connected to a different parent
						if (otherNode != NULL && otherNode->defectedparent != node->defectedparent)
						{
							//DEBUG_PRINT(outputFile, "Other node %d\n", otherNode->number);
							//retract the edge
							edgesWeight_1[node->edgesNear[k]] += MAXWEIGHT / 2;
							//edge->weight += MAXWEIGHT / 2;


						}

					}
				}
				node->weightGot -= MAXWEIGHT / 2;
				nodes[nCount] = pos;
				nCount++;

				//DEBUG_PRINT(outputFile, "Node defected parent %d\n", node->defectedparent);
				//DEBUG_PRINT(outputFile, "Node blossom parent %d\n", node->blossomParent);
			}
			else {
				//DEBUG_PRINT(outputFile, "Node %d is already retracted\n", node->number);
				//if connected to node with same parent and not in nodes then add it
				//DEBUG_PRINT(outputFile, "%d ? %d && %d ? %d", defectedNodes_1[node->defectedparent].x, node->x, defectedNodes_1[node->defectedparent].y, node->y);
				if (defectedNodes_1[node->defectedparent].x != node->x || defectedNodes_1[node->defectedparent].y != node->y)
				{
					for (k = 0; k < 4; k++)
					{
						//must be maxweight
						if (node->edgesNear[k] != -1 && edgesWeight_1[node->edgesNear[k]] == 0)
						{
							//check if the other node is connected to the same parent
							Node* otherNode = getOtherNode(k, node, d);
							if (otherNode != NULL && otherNode->defectedparent != -1 && otherNode->defectedparent == node->defectedparent)
							{
								//LOCK UNTIL ADDED TO THE LIST
								hlsint isIn = -1;
								//check if is the first time i see the node
								for (hlsint l = 0; l < nCount && isIn ==-1; l++)
								{
									if (nodes[l].x == otherNode->number % (d + 1) && nodes[l].y == otherNode->number / (d + 1))
									{
										isIn = 0;
									}
								}
								if (isIn == -1)
								{
									nodes[nCount].x = otherNode->number % (d + 1);
									nodes[nCount].y = otherNode->number / (d + 1);
									nCount++;
									//UNLOCK
									//DEBUG_PRINT(outputFile, "Adding node %d to be retracted next\n", otherNode->number);
									otherNode->weightGot -= MAXWEIGHT / 2;
									//DEBUG_PRINT(outputFile, "Node %d reduced weight\n", otherNode->number);
									for (hlsint h = 0; h < 4; h++)
									{
										hlsint edge1 = edgesWeight_1[otherNode->edgesNear[h]];
										if (otherNode->edgesNear[h] != -1 && edge1 == 0)
										{
											edgesWeight_1[otherNode->edgesNear[h]] += MAXWEIGHT / 2;
										}
									}

								}
							}
						}
					}
					//DEBUG_PRINT(outputFile, "Node number %d \n", node->number);
					//DEBUG_PRINT(outputFile, "Defected Node number %d \n", graph_1[defectedNodes_1[defectedNode].y][defectedNodes_1[defectedNode].x].number);

					if (node != &graph_1[defectedNodes_1[defectedNode].y][defectedNodes_1[defectedNode].x])
					{
						node->defectedparent = -1;
						hlsbool done = 0;
						//if there is an edge with weight 0 then the node must be set as some of the other nodes
						for (k = 0; k < 4 && !done; k++)
						{
							if (node->edgesNear[k] != -1 && edgesWeight_1[node->edgesNear[k]] == 0)
							{
								Node* otherNode = getOtherNode(k, node, d);
								if (otherNode != NULL && otherNode->defectedparent != -1)
								{
									//add the node to the expansion list
									defectedNodes_1[otherNode->defectedparent].nodes[defectedNodes_1[otherNode->defectedparent].nodesCount].x = pos.x;
									defectedNodes_1[otherNode->defectedparent].nodes[defectedNodes_1[otherNode->defectedparent].nodesCount].y = pos.y;
									defectedNodes_1[otherNode->defectedparent].nodesCount++;
									node->defectedparent = otherNode->defectedparent;
									//Assigned the highest parent to the node else the defected node

									done=true;
								}
							}
						}
					}
				}
				else {
					//DEBUG_PRINT(outputFile, "Node %d is a defected node, cannot retract\n", node->number);
					nodes[nCount] = pos;
					nCount++;
				}
			}
		}
	}
	for (j = 0; j < MAXNODESCOUNT; j++)
	{
		if (j < nCount)
		{
			defectedNodes_1[defectedNode].nodes[j].x = nodes[j].x;
			defectedNodes_1[defectedNode].nodes[j].y = nodes[j].y;
		}
	}
	defectedNodes_1[defectedNode].nodesCount = nCount;
	//Delete all canTouch connections
	//DEBUG_PRINT(outputFile, "Deleting all canTouch connections of node %d\n", defectedNode);
	for (j = 0; j < MAXCANTOUCHES; j++)
	{
		if (j < defectedNodes_1[defectedNode].canTouchCount)
		{

			hlsint otherNode = defectedNodes_1[defectedNode].canTouch[j].node;
			//DEBUG_PRINT(outputFile, "Deleting connection between %d and %d\n", defectedNode, otherNode);
			//DEBUG_PRINT all canTouch connections of other node
			deleteOtherCanTouch(defectedNode, otherNode);
		}
	}
	defectedNodes_1[defectedNode].canTouchCount = 0;
	defectedNodes_1[defectedNode].lastRoundModified = actualRound_1;
}
/**
 * if the defected node must be expanded:
 * expand in the model graph ( sub MAXWEIGHT/2 to near edges),
 * if remaining weight==0 ->
 * 	if other node has a defectedParent (is under another defected node) and is not already touching the one exapnding
 * 		call touches processing to create the connection in the syndrome graph
 * 	else the defected node becomes the defected parent of the other node in the model graph
 */
void processExpandDefectedNodes(hlsint d, hlsint defectedNode)//, TouchesExpansion touchesForExpansion[], hlsint* touchesForExpansionCount)
{
#pragma HLS INLINE off
	hlsbool expanded = false;
	hlsint count = defectedNodes_1[defectedNode].nodesCount;
	hlsint j;
	hlsint firstParent = -1;
	hlsint secondParent = -1;
	hlsint firstChild = -1;
	hlsint secondChild = -1;
	nodeToChangeCount_1 = 0;
	//else continue with the new ones
	for (j = 0; j < count; j++)
	{
		NodePosition pos = defectedNodes_1[defectedNode].nodes[j];
		hlsint edgeSum = 0;
		if (pos.x != -1 && pos.y != -1)
		{
			//fprintf(outputFile, "Expand node %d\n", graph_1[pos.y][pos.x].number);
			Node* node = &graph_1[pos.y][pos.x];
			//DEBUG_PRINT(outputFile, "Expand node %d\n", node->number);
			if (!node->isVirtual)
			{
				for (hlsint k = 0; k < 4; k++)
				{
					if (node->edgesNear[k] != -1 && edgesWeight_1[node->edgesNear[k]] != 0)
					{
						//DEBUG_PRINT(outputFile,"Node %d, Edge %d, Weight %d\n", node->number, edge->number, edge->weight);
						edgesWeight_1[node->edgesNear[k]] -= MAXWEIGHT / 2;
						edgeSum += edgesWeight_1[node->edgesNear[k]];
						if (edgesWeight_1[node->edgesNear[k]] == 0)
						{
							Node* otherNode = getOtherNode(k, node, d);
							if (otherNode->defectedparent != -1 && (node->weightGot == 0 || defectedNodes_1[otherNode->defectedparent].x != otherNode->x || defectedNodes_1[otherNode->defectedparent].y != otherNode->y || defectedNodes_1[otherNode->defectedparent].hasParent))
							{
								if (defectedNodes_1[node->defectedparent].hasParent)
									firstParent = defectedNodes_1[node->defectedparent].highestParent;
								else
									firstParent = node->defectedparent;
								if (defectedNodes_1[otherNode->defectedparent].hasParent)
									secondParent = defectedNodes_1[otherNode->defectedparent].highestParent;
								else
									secondParent = otherNode->defectedparent;
								firstChild = node->defectedparent;
								secondChild = otherNode->defectedparent;
								//hlsint lastTouch = *touchesForExpansionCount - 1;
								if (firstParent != secondParent && (defectedNodes_1[secondParent].touchCount == 0 || (defectedNodes_1[secondParent].touchCount == 1 && firstParent != defectedNodes_1[secondParent].touch[0].node) || (defectedNodes_1[secondParent].touchCount == 2 && firstParent != defectedNodes_1[secondParent].touch[1].node && firstParent != defectedNodes_1[secondParent].touch[0].node)))
								{
									touchesProcessing(firstParent, secondParent, firstChild, secondChild);
								}

							}
							else if (otherNode->defectedparent != -1)
							{
								//DEBUG_PRINT(outputFile, "The node in contact is a defected node serching for near defected nodes\n");
								/*for (hlsint i = 0; i < 4; i++) {
									if (i != (3 - k)) {

									if (otherNode->edgesNear[i] != -1 && edgesWeight_1[otherNode->edgesNear[i]] == 0) {
											Node* otherOtherNode = getOtherNode(i, otherNode, d);
											if (otherOtherNode != NULL && otherOtherNode->defectedparent != -1)
											{
												if (defectedNodes_1[node->defectedparent].hasParent)
													firstParent = defectedNodes_1[node->defectedparent].highestParent;
												else
													firstParent = node->defectedparent;
												if (defectedNodes_1[otherOtherNode->defectedparent].hasParent)
													secondParent = defectedNodes_1[otherOtherNode->defectedparent].highestParent;
												else
													secondParent = otherOtherNode->defectedparent;
												firstChild = node->defectedparent;
												secondChild = otherOtherNode->defectedparent;
												//hlsint lastTouch = *touchesForExpansionCount - 1;
												if (firstParent != secondParent && (defectedNodes_1[secondParent].touchCount == 0 || (defectedNodes_1[secondParent].touchCount == 1 && firstParent != defectedNodes_1[secondParent].touch[0].node) || (defectedNodes_1[secondParent].touchCount == 2 && firstParent != defectedNodes_1[secondParent].touch[1].node && firstParent != defectedNodes_1[secondParent].touch[0].node)))
												{
													touchesProcessing(firstParent, secondParent, firstChild, secondChild);
												}
											}
										}
									}
								}*/
								if (defectedNodes_1[otherNode->defectedparent].touch[0].otherchild==defectedNode || defectedNodes_1[otherNode->defectedparent].touch[1].otherchild == defectedNode)
{
				touchesProcessing(defectedNodes_1[otherNode->defectedparent].touch[0].node, defectedNodes_1[otherNode->defectedparent].touch[0].otherchild, defectedNodes_1[otherNode->defectedparent].touch[1].node, defectedNodes_1[otherNode->defectedparent].touch[1].otherchild);
}
							}
							else
							{
								//DEBUG_PRINT(outputFile, "Defected node %d got %d connected\n", defectedNode, otherNode->number);
								otherNode->defectedparent = node->defectedparent;
								//TODO CHECK IF CORRECT PARENT
								//Assigned the highest parent to the node else the defected node
								if (!otherNode->isVirtual)
								{
									//fprintf(outputFile, "Defected node %d connected to defected node %d\n", defectedNode, otherNode->number);
									nodeToChange_1[nodeToChangeCount_1].x = otherNode->number % (d + 1);
									nodeToChange_1[nodeToChangeCount_1].y = otherNode->number / (d + 1);
									nodeToChangeCount_1++;
								}
								else {
									defectedNodes_1[defectedNode].connectedToVirtual = true;
									updated_1 = true;
									//also highest parent connected to virtual
									if (defectedNodes_1[defectedNode].hasParent)
									{
										defectedNodes_1[defectedNodes_1[defectedNode].highestParent].connectedToVirtual = true;
										//DEBUG_PRINT(outputFile, "Highest parent %d connected to virtual\n", defectedNodes_1[defectedNode].highestParent);
									}
									//DEBUG_PRINT(outputFile, "Defected node %d connected to virtual\n", defectedNode);
								}
								expanded = true;
							}
						}
					}
				}
				if (edgeSum == 0 && expanded)
				{
					defectedNodes_1[defectedNode].nodes[j].x = -1;
					defectedNodes_1[defectedNode].nodes[j].y = -1;
				}
				node->weightGot += MAXWEIGHT / 2;
			}
		}
	}
	//print all nodes to be expanded next
	//DEBUG_PRINT(outputFile, "Nodes to be expanded next:\n");
	//fprintf(outputFile, "Defected node %d dimension %d expanded\n", defectedNode, defectedNodes_1[defectedNode].nodesCount);
	//TODO DELETE
	hlsint newCount = 0;
	for (j = 0; j < MAXNODESCOUNT; j++)
	{
		if (j < defectedNodes_1[defectedNode].nodesCount)
		{
			if (defectedNodes_1[defectedNode].nodes[j].x != -1 && defectedNodes_1[defectedNode].nodes[j].y != -1)
			{
				defectedNodes_1[defectedNode].nodes[newCount].x = defectedNodes_1[defectedNode].nodes[j].x;
				defectedNodes_1[defectedNode].nodes[newCount].y = defectedNodes_1[defectedNode].nodes[j].y;
				newCount++;
			}
		}
	}
	hlsint newNumber = 0;
	for (j = 0; j < nodeToChangeCount_1; j++)
	{
		defectedNodes_1[defectedNode].nodes[newCount].x = nodeToChange_1[j].x;
		defectedNodes_1[defectedNode].nodes[newCount].y = nodeToChange_1[j].y;
		newCount++;
	}
	defectedNodes_1[defectedNode].nodesCount = newCount;
	//fprintf(outputFile, "Defected node %d dimension %d expanded\n", defectedNode, defectedNodes_1[defectedNode].nodesCount);
	hlsint defectedValue = defectedNodes_1[defectedNode].highestParent;
	//if i created a blossom this actualRound_1 i need to update all remaining nodes to the highest parent

	defectedNodes_1[defectedNode].lastRoundModified = actualRound_1;

}
/**
 * Deletion of a blossom when dimension == 0 and next round must be retracted:
 * 	Reset all highest parent of children nodes to the remaining highest blossom
 * Decide on how the blossom should be divided (wher to cut internal connections): 1 pair and one node added to the chain or all nodes added to the chain
 * Delete the touches with external nodes and add them to the new highest paretn of the children node of the connection
 *
 */
/*
void deleteBlossom(hlsint defectedNode)
{
#pragma HLS INLINE off
	Blossom* blossom = &blossoms_1[defectedNodes_1[defectedNode].myself];
	hls::stream<hlsint> queue;
#pragma HLS stream variable=queue depth=MAX_QUEUE_SIZE
	hlsint defectedposition = blossom->myself;
	hlsint firstDefectedNode = defectedNodes_1[defectedposition].touch[0].mychild;
	hlsint secondDefectedNode = defectedNodes_1[defectedposition].touch[1].mychild;;
	hlsint dimension = blossom->childrenBlossomCount + blossom->childrenDefectedNodesCount;
	hlsint i, j, k;
	hlsint blossomPosition = blossom->myself;
	//TODO
	//update the parent of the defected nodes
	for (i = 0; i < MAXBLOSSOMSDIMENSION; i++)
	{
#pragma HLS PIPELINE off
#pragma HLS UNROLL factor=1
		//MAX AT 5
		hlsint defectedNodeValue = blossom->childrenDefectedNodes[i];
		defectedNodes_1[defectedNodeValue].hasParent = false;
		defectedNodes_1[defectedNodeValue].highestParent = -1;
		defectedNodes_1[defectedNodeValue].directParent = -1;
		//for each node for expansion of the defected node the blossomParent is the defected node
	}
	//put each blossom
//CREATE A PAIR and the node with both connections should be add to the chain
	if (firstDefectedNode == secondDefectedNode) {
		//cut both touch of firstDefectedNode
		hlsint defectedTouch0 = defectedNodes_1[firstDefectedNode].touch[0].node;
		hlsint defectedTouch1 = defectedNodes_1[firstDefectedNode].touch[1].node;
		deleteTouch(firstDefectedNode, defectedTouch1, false);
		deleteTouch(firstDefectedNode, defectedTouch0, false);
		defectedNodes_1[defectedTouch0].expandOrReduce = 0;
		defectedNodes_1[defectedTouch1].expandOrReduce = 0;
		//put all the nodes in the chain to expand,reduce =0
		defectedNodes_1[defectedTouch0].touch[0].touchType = STRONG;
		defectedNodes_1[defectedTouch1].touch[0].touchType = STRONG;
		//connect firstNode with the nodes connected to the father
	}
	else {
		//cut the edge between firstDefectedNode and secondDefectedNode
		//DEBUG_PRINT(outputFile, "Cutting the edge between %d and %d\n", firstDefectedNode, secondDefectedNode);
		deleteTouch(firstDefectedNode, secondDefectedNode, false);
	}
	//else
		//DEBUG_PRINT(outputFile, "NOT DONE: Dimension > 3\n");

	//ALL NODES are part of the chain

	hlsint defectedTouch = defectedNodes_1[blossomPosition].touch[1].node;
	hlsint defectedTouchOtherChild = defectedNodes_1[blossomPosition].touch[1].otherchild;
	hlsint defectedTouchMyChild = defectedNodes_1[blossomPosition].touch[1].mychild;
	hlsbool touchType = defectedNodes_1[blossomPosition].touch[1].touchType;
	//delete the touch
	deleteTouch(blossomPosition, defectedTouch, true);
	//DEBUG_PRINT(outputFile, "Connecting %d with %d with touchType %d\n", defectedTouch, firstDefectedNode, touchType);
	connectNodes(firstDefectedNode, defectedTouch, defectedTouchMyChild, defectedTouchOtherChild, touchType);
	defectedTouch = defectedNodes_1[blossomPosition].touch[0].node;
	defectedTouchOtherChild = defectedNodes_1[blossomPosition].touch[0].otherchild;
	defectedTouchMyChild = defectedNodes_1[blossomPosition].touch[0].mychild;
	touchType = defectedNodes_1[blossomPosition].touch[0].touchType;
	//delete the touch
	deleteTouch(blossomPosition, defectedTouch, true);
	//DEBUG_PRINT(outputFile, "Connecting %d with %d with touchType %d\n", defectedTouch, secondDefectedNode, touchType);
	connectNodes(secondDefectedNode, defectedTouch, defectedTouchMyChild, defectedTouchOtherChild, touchType);
	defectedNodes_1[defectedNode].usable = false;



	//	TODO DECISION ON WHICH EDGE TO CUT
	//	FOR BOTH CONNECTIONS OF THE BLOSSOM
	//	USING ALL THE BLOSSOM EXTERNAL CONNECTIONS UNITE THE OTHER NODE WITH THE HIGHEST PARENT LEFT (THE FIRST CHILDREN OF THE BLOSSOM)
	//	-> NEED TO MODIFY THE OTHER CONNECTIONS SO IT KNOWS IT IS ALREADY CONNECTED TO HIM
	//	-> NEED TO MODIFY THE BLOSSOM CHILDREN SO THEY KNOW THEY ARE CONNECTED TO THE OTHER NODE 
}
*/
/**
 * Check if the node connected to virtual is actually good to stabilize the chain
 * is stable if in both part of the chain there are an even number of elements
 */
hlsbool checkGoodVirtual(hlsint connectedToVirtual) {
#pragma HLS INLINE off
	//DEBUG_PRIORITY_PRINT(outputFile, "Check good virtual\n");
	hlsint i, j;
	if (defectedNodes_1[connectedToVirtual].touchCount == 1 || defectedNodes_1[connectedToVirtual].touchCount == 0)
		return true;
	else {
		hlsint firstNode = -1;
		hlsint lastNode = connectedToVirtual;
		hlsint currentNode = -1;
		int count = 0;
		
		for (j = 0; j < MAXTOUCHES; j++)
		{
			
			currentNode = defectedNodes_1[connectedToVirtual].touch[j].node;
		findLastLoop:do {
#pragma HLS loop_tripcount min=2 max=5 avg=3
			// Ignore the return touch of the next node that is the same as the previous
			if (defectedNodes_1[currentNode].touch[0].node != lastNode)
			{
				//DEBUG_PRINT(outputFile, "Added node %d to the queue\n", defectedNodes_1[currentNode].touch[0].node);
				lastNode = currentNode;
				currentNode = defectedNodes_1[currentNode].touch[0].node;
			}
			else if (defectedNodes_1[currentNode].touchCount == MAXTOUCHES)
			{
				//DEBUG_PRINT(outputFile, "Added node %d to the queue\n", defectedNodes_1[currentNode].touch[1].node);
				lastNode = currentNode;
				currentNode = defectedNodes_1[currentNode].touch[1].node;
			}
			else {
				lastNode = currentNode;
				currentNode = -1;
				//break;
			}
			count++;
		} while (currentNode != -1);
		if (count % 2 == 1)
		{
			//DEBUG_PRIORITY_PRINT(outputFile, "Node %d has an odd number of nodes in the chain\n", firstNode);
			return false;
		}
		count = 0;
		}

	}
	return true;
}
/**
 * At the end of each round check if all the nodes are stable:
 * if they are in an even number chain or has a node connected correctly to a virtual node
 */
hlsbool roundcheckWithQueue()
{
	//DEBUG_PRINT(outputFile, "ActualRound check with queue\n");
	hlsint i;
	hlsint usedNodes[MAXDEFECTEDNODES];
	hls::stream<hlsint> queue;
#pragma HLS stream variable=queue depth=MAX_QUEUE_SIZE

	hlsbool needExpansion = false;
	hlsbool hasVirtual = false;

	for (i = 0; i < MAXDEFECTEDNODES; i++)
	{
		if (i < defectedNodesCount_1)
		{
			if (!defectedNodes_1[i].usable || defectedNodes_1[i].hasParent)
				usedNodes[i] = -2;
			else
				usedNodes[i] = -1;
		}
	}
	for (i = 0; i < MAXDEFECTEDNODES; i++)
	{
		if (i < defectedNodesCount_1)

		{
			// eliminate array of pointers
			hlsint defectedNode = i;
			hlsint count = 1;
			//Check if the node is not used
			// if the defected node is not used and has no parent then add it to the stack
			// Only top level nodes are considered in the execution
			if (defectedNodes_1[defectedNode].usable && usedNodes[i] == -1 && defectedNodes_1[defectedNode].touchCount < 2)
			{
				//DEBUG_PRINT(outputFile, "Checking node %d\n", defectedNode);
				//start from top level node in the chain
				hasVirtual = false;
				queue.write(defectedNode);
				hlsint lastNode = defectedNode;
				usedNodes[defectedNode] = 0;
				while (!queue.empty())
				{
					hlsint currentNode = queue.read();
					if (defectedNodes_1[currentNode].connectedToVirtual && !hasVirtual)
						hasVirtual = checkGoodVirtual(currentNode);
					//setup for non expansion
					defectedNodes_1[currentNode].expandOrReduce = 0;
					for (hlsint j = 0; j < MAXTOUCHES; j++)
					{
						if (j < defectedNodes_1[currentNode].touchCount)
						{
							hlsint nextNode = defectedNodes_1[currentNode].touch[j].node;
							// Ignore the return touch of the next node that is the same as the previous
							if (nextNode != lastNode)
							{
								count++;
								usedNodes[nextNode] = usedNodes[currentNode] + 1;
								//print all what was done
								//DEBUG_PRINT(outputFile, "Added node %d to the queue\n", nextNode);
								queue.write(nextNode);
							}
						}
					}
					lastNode = currentNode;
				}
				if (count % 2 == 1 && !hasVirtual)
				{
					//DEBUG_PRINT(outputFile, "Node %d has an odd number of nodes in the chain\n", defectedNode);
					needExpansion = true;
					queue.write(defectedNode);
					defectedNodes_1[defectedNode].expandOrReduce = 1;
					lastNode = defectedNode;
					while (!queue.empty())
					{
						hlsint currentNode = queue.read();
						for (hlsint h = 0; h < MAXTOUCHES; h++)
						{
							if (h < defectedNodes_1[currentNode].touchCount)
							{
								hlsint nextNode = defectedNodes_1[currentNode].touch[h].node;
								// Ignore the return touch of the next node that is the same as the previous
								if (nextNode != lastNode)
								{
									if (defectedNodes_1[currentNode].expandOrReduce == -1)
									{
										defectedNodes_1[nextNode].expandOrReduce = 1;
										//print all what was done
										//DEBUG_PRINT(outputFile, "Added node %d to the queue\n", nextNode);
										queue.write(nextNode);
									}
									else {
										if (defectedNodes_1[nextNode].isBlossom && blossoms_1[defectedNodes_1[nextNode].myself].dimension == 0) {
											//DEBUG_PRIORITY_PRINT(outputFile, "Node %d is a blossom with dimension 0\n", nextNode);
											//deleteBlossom(nextNode);
											error=4;
											//h = -1;
										}
										else {
											defectedNodes_1[nextNode].expandOrReduce = -1;
											//print all what was done
											//DEBUG_PRINT(outputFile, "Added node %d to the queue\n", nextNode);
											queue.write(nextNode);
										}
									}
								}

							}
						}
						lastNode = currentNode;
					}
				}
			}
		}
	}

	//printAllElements();
	return needExpansion;
}
/**
 * find all Strong connection in the syndrome graph inside the blossom and add them to the solutipon pairs
 */
void findAllBlossomStrongConnection(hlsint parent, PairDNode* pairs, hlsint* pairCount, hlsint d) {
#pragma HLS INLINE off
	hls::stream<hlsint> queue;
#pragma HLS stream variable=queue depth=MAX_QUEUE_SIZE

	hlsint j, k, l;
	hlsint currentBlossom = defectedNodes_1[parent].myself;
	//put each blossom children in the queue
	//DEBUG_PRINT(outputFile, "Going to defected nodes\n");
	//put each defectedNode children in defectedNodes
	hlsint defectedNode = blossoms_1[currentBlossom].childrenDefectedNodes[0];
	hlsint toPair1;
	hlsint toPair2;
	if(defectedNodes_1[defectedNode].touch[0].touchType == STRONG){
		toPair1=defectedNode;
		toPair2= defectedNodes_1[defectedNode].touch[0].otherchild;
	}
	else if(defectedNodes_1[defectedNode].touch[1].touchType == STRONG){
		toPair1=defectedNode;
		toPair2= defectedNodes_1[defectedNode].touch[1].otherchild;
	}
	else{
		toPair1= blossoms_1[currentBlossom].childrenDefectedNodes[1];
		toPair2= blossoms_1[currentBlossom].childrenDefectedNodes[2];
	}
	pairs[*pairCount].node1 = defectedNodes_1[toPair1].y * (d + 1) + defectedNodes_1[toPair1].x;
	pairs[*pairCount].node2 = defectedNodes_1[toPair2].y * (d + 1) + defectedNodes_1[toPair2].x;
	//DEBUG_PRINT(outputFile, "Adding pair %d %d\n", pairs[*pairCount].node1, pairs[*pairCount].node2);
	(*pairCount)++;
	
	/*
	for (j = 0; j < blossoms_1[currentBlossom].childrenDefectedNodesCount; j++)
	{
		hlsint defectedNode = blossoms_1[currentBlossom].childrenDefectedNodes[j];
		for (k = 0; k < MAXTOUCHES; k++)
		{
			TouchDefectedNode blossomDefectedT = defectedNodes_1[defectedNode].touch[k];
			if (defectedNodes_1[defectedNode].touch[k].touchType == STRONG)
			{
				for (l = 0; l < blossoms_1[currentBlossom].childrenDefectedNodesCount; l++)
				{
					if (blossoms_1[currentBlossom].childrenDefectedNodes[l] == blossomDefectedT.node && l > j)
					{
						pairs[*pairCount].node1 = defectedNodes_1[blossomDefectedT.mychild].y * (d + 1) + defectedNodes_1[blossomDefectedT.mychild].x;
						pairs[*pairCount].node2 = defectedNodes_1[blossomDefectedT.otherchild].y * (d + 1) + defectedNodes_1[blossomDefectedT.otherchild].x;
						//DEBUG_PRINT(outputFile, "Adding pair %d %d\n", pairs[*pairCount].node1, pairs[*pairCount].node2);
						(*pairCount)++;
					}
				}
			}
		}
	}
	*/
}
/**
 * Construct the pairs: if one of the node is a blossom
 * calls findAllBlossomStrongConnection to create the pairs inside of the blossom
 */
void constructionOfPairs(hlsint defectedStrongNode, PairDNode* pairs, hlsint* pairsCount, hlsint d) {
#pragma HLS INLINE off
	hls::stream<hlsint> queue;
#pragma HLS stream variable=queue depth=MAX_QUEUE_SIZE

	queue.write(defectedStrongNode);
	hlsint lastNode = defectedStrongNode;
	while (!queue.empty())
	{
		hlsint currentNode = queue.read();
		for (hlsint i = 0; i < MAXTOUCHES; i++)
		{
			if (i < defectedNodes_1[currentNode].touchCount)
			{
				TouchDefectedNode touch = defectedNodes_1[currentNode].touch[i];
				hlsint nextNode = touch.node;
				// Ignore the return touch of the next node that is the same as the previous
				if (nextNode != lastNode)
				{
					//if the connection is STRONG ADD TO PAIRS
					if (touch.touchType == STRONG)
					{
						//DEBUG_PRINT(outputFile, "Adding pair %d %d\n", defectedNodes_1[currentNode].touch[i].mychild, defectedNodes_1[currentNode].touch[i].otherchild);
						pairs[*pairsCount].node1 = defectedNodes_1[touch.mychild].y * (d + 1) + defectedNodes_1[touch.mychild].x;
						pairs[*pairsCount].node2 = defectedNodes_1[touch.otherchild].y * (d + 1) + defectedNodes_1[touch.otherchild].x;
						(*pairsCount)++;
						if (defectedNodes_1[currentNode].isBlossom)
							findAllBlossomStrongConnection(currentNode, pairs, pairsCount, d);

						if (defectedNodes_1[nextNode].isBlossom)
							findAllBlossomStrongConnection(nextNode, pairs, pairsCount, d);

					}
					queue.write(nextNode);
				}
			}

		}
		lastNode = currentNode;
	}
}
/**
 * If a Blossom is connected to a virtual node this method is called
 * return the defected node that is connected to the virtual node
 */
void findVirtualInnerNode(hlsint toVirtual, PairDNode* pairs, hlsint* pairsCount, hlsint d) {
#pragma HLS INLINE off
	//Remember to do a strong connection between the virtual node and the inner node
	hlsint blossomPosition = defectedNodes_1[toVirtual].myself;
	hlsint i, j;
	hlsint position = -1;
	hls::stream<hlsint> queue;
#pragma HLS stream variable=queue depth=MAX_QUEUE_SIZE

	//check if direct defected node is connected to the virtual node
	for (i = 0; i < blossoms_1[blossomPosition].childrenDefectedNodesCount && position == -1; i++)
	{
		if (defectedNodes_1[blossoms_1[blossomPosition].childrenDefectedNodes[i]].connectedToVirtual)
		{
			//printf("Defected node %d is connected to virtual\n", blossoms_1[blossomPosition].childrenDefectedNodes[i]->number);
			position = blossoms_1[blossomPosition].childrenDefectedNodes[i];
			modifyOnlyMyInternalConnections(defectedNodes_1[position].highestParent, position);
		}
	}
	if (position != -1) {
		pairs[*pairsCount].node1 = defectedNodes_1[position].y * (d + 1) + defectedNodes_1[position].x;
		if (defectedNodes_1[position].x * 2 < d) {
			pairs[*pairsCount].node2 = defectedNodes_1[position].y * (d + 1);
		}
		else
		{
			pairs[*pairsCount].node2 = defectedNodes_1[position].y * (d + 1) + d;
		}
		(*pairsCount)++;
	}
	//else
		//DEBUG_PRINT(outputFile, "ERROR during finding the connection in the Blossom for the virtual node\n");
}
/**
 * method that check the connections in the Syndrome graph:
 * if a chain is connected to virtual consider separately the node connected to the virtual
 * and the possible 2 reamining part of the chain
 */
void createFinalConnections(hlsint d, PairDNode* pairs, hlsint* pairsCount)
{
#pragma HLS INLINE off
	//DEBUG_PRIORITY_PRINT(outputFile, "\n");
	//DEBUG_PRIORITY_PRINT(outputFile, "Final connections\n");
	hlsint usedNodes[MAXDEFECTEDNODES];
	hls::stream<hlsint> queue;
#pragma HLS stream variable=queue depth=MAX_QUEUE_SIZE

	hlsint i, j;
	(*pairsCount) = 0;
	for (i = 0; i < defectedNodesCount_1; i++)
	{
		if (!defectedNodes_1[i].usable || defectedNodes_1[i].hasParent)
			usedNodes[i] = -2;
		else
			usedNodes[i] = -1;
	}
	for (i = 0; i < defectedNodesCount_1; i++)
	{
#pragma HLS UNROLL factor=1
		// eliminate array of pointers
		hlsint defectedNode = i;
		hlsint count = 1;
		hlsint isVirtual = -1;
		//Check if the node is not used
		// if the defected node is not used and has no parent then add it to the stack
		// Only top level nodes are considered in the execution
		if (defectedNodes_1[defectedNode].usable && usedNodes[defectedNode] == -1 && defectedNodes_1[defectedNode].touchCount < 2)
		{
			//DEBUG_PRINT(outputFile, "Checking node %d\n", defectedNode);
			//start from top level node in the chain
			queue.write(defectedNode);
			hlsint lastNode = defectedNode;
			usedNodes[defectedNode] = 0;
			while (!queue.empty())
			{
				hlsint currentNode = queue.read();
				if (defectedNodes_1[currentNode].connectedToVirtual && checkGoodVirtual(currentNode)) {
					if (isVirtual == -1)
						isVirtual = currentNode;
					else if (defectedNodes_1[isVirtual].isBlossom)
						//need to symplify the most possible
						isVirtual = currentNode;
				}
				//setup for non expansion
				//defectedNodes_1[currentNode].expandOrReduce = 0;
				for (j = 0; j < MAXTOUCHES; j++)
				{
					if (j < defectedNodes_1[currentNode].touchCount)
					{
						hlsint nextNode = defectedNodes_1[currentNode].touch[j].node;
						// Ignore the return touch of the next node that is the same as the previous
						if (nextNode != lastNode)
						{
							count++;
							usedNodes[defectedNodes_1[currentNode].touch[j].node] = usedNodes[currentNode] + 1;
							//print all what was done
							//DEBUG_PRINT(outputFile, "Added node %d to the queue\n", nextNode);
							queue.write(nextNode);
						}
					}
				}
				lastNode = currentNode;
			}

			if (count % 2 == 1)
			{
				hlsint defectedStrongNode = -1;
				hlsint defectedWeakNode = -1;
				//DEBUG_PRINT(outputFile, "Node %d has an odd number of nodes in the chain\n", defectedNode);
				//BAD GET VIRTUAL NODE
				//DEBUG_PRINT(outputFile, "THe node %d has %d touches\n", isVirtual, defectedNodes_1[isVirtual].touchCount);
				if (defectedNodes_1[isVirtual].touchCount == 2)
				{
					/*
					//find the node connected with a WEAK connection and delete the connection
					for (j = 0; j < MAXTOUCHES; j++)
					{
						if (j < defectedNodes_1[isVirtual].touchCount)
						{
							TouchDefectedNode touch = defectedNodes_1[isVirtual].touch[j];
							if (touch.touchType == STRONG)
							{
								//delete the connection
								//DEBUG_PRINT(outputFile, "Deleting strong connection between %d and %d\n", isVirtual, defectedNodes_1[isVirtual].touch[j].node);
								defectedStrongNode = touch.node;
								deleteTouch(isVirtual, touch.node, true);
							}
							else {
								//delete the connection
								//DEBUG_PRINT(outputFile, "Deleting weak connection between %d and %d\n", isVirtual, defectedNodes_1[isVirtual].touch[j].node);
								defectedWeakNode = touch.node;
								deleteTouch(isVirtual, touch.node, true);
							}
						}
					}*/
					if (defectedNodes_1[isVirtual].touch[1].touchType == STRONG)
					{
						defectedStrongNode = defectedNodes_1[isVirtual].touch[1].node;
						deleteTouch(isVirtual, defectedNodes_1[isVirtual].touch[1].node, true);
						defectedWeakNode = defectedNodes_1[isVirtual].touch[0].node;
						deleteTouch(isVirtual, defectedNodes_1[isVirtual].touch[0].node, true);
					}
					else {
						defectedWeakNode = defectedNodes_1[isVirtual].touch[1].node;
						deleteTouch(isVirtual, defectedNodes_1[isVirtual].touch[1].node, true);
						defectedStrongNode = defectedNodes_1[isVirtual].touch[0].node;
						deleteTouch(isVirtual, defectedNodes_1[isVirtual].touch[0].node, true);
					}
				}
				else if (defectedNodes_1[isVirtual].touchCount == 1) {
					if (defectedNodes_1[isVirtual].touch[0].touchType == STRONG)
					{
						//delete the connection
						defectedStrongNode = defectedNodes_1[isVirtual].touch[0].node;
					}
					else {
						defectedWeakNode = defectedNodes_1[isVirtual].touch[0].node;
					}
					deleteTouch(isVirtual, defectedNodes_1[isVirtual].touch[0].node, true);
				}
				if (defectedStrongNode != -1) {

					//DEBUG_PRINT(outputFile, "The node %d had a strong connection\n", defectedStrongNode);
					changeAllInChain(defectedStrongNode);
					constructionOfPairs(defectedStrongNode, pairs, pairsCount, d);
				}
				if (defectedWeakNode != -1)
				{
					//DEBUG_PRINT(outputFile, "The node %d had a weak connection\n", defectedWeakNode);
					constructionOfPairs(defectedWeakNode, pairs, pairsCount, d);
				}
				if (defectedNodes_1[isVirtual].isBlossom)
				{
					//DEBUG_PRINT(outputFile, "The node connected to the virtual node is a blossom\n");
					findVirtualInnerNode(isVirtual, pairs, pairsCount, d);
					findAllBlossomStrongConnection(isVirtual, pairs, pairsCount, d);
				}
				else {
					pairs[*pairsCount].node1 = defectedNodes_1[isVirtual].y * (d + 1) + defectedNodes_1[isVirtual].x;
					if (defectedNodes_1[isVirtual].x * 2 < d) {
						pairs[*pairsCount].node2 = defectedNodes_1[isVirtual].y * (d + 1);
					}
					else
					{
						pairs[*pairsCount].node2 = defectedNodes_1[isVirtual].y * (d + 1) + d;
					}
					(*pairsCount)++;
				}
			}
			else {
				//DEBUG_PRINT(outputFile, "Node %d has a stable chain\n", defectedNode);
				constructionOfPairs(defectedNode, pairs, pairsCount, d);
			}
		}
	}
}


void resolve(hlsint nDefNodes, hlsint dimension, hlsint defectedN[], PairDefectedNode pairsTest[], hlsint* pairsTestCount) {

	//#pragma HLS ALLOCATION function instances= processRetractDefectedNodes limit=1
	//#pragma HLS ALLOCATION function instances= processExpandDefectedNodes limit=1
	//#pragma HLS ALLOCATION function instances= roundcheckWithQueue limit=1

#pragma HLS INTERFACE mode=m_axi port=pairsTest offset=slave bundle=gmem0 depth=2*MAXELEMENTS
#pragma HLS INTERFACE mode=m_axi port=pairsTestCount offset=slave bundle=gmem1 depth=1
#pragma HLS INTERFACE mode=m_axi port=defectedN offset=slave bundle=gmem2 depth=2*MAXELEMENTS
#pragma HLS INTERFACE s_axilite port=nDefNodes bundle=control
#pragma HLS INTERFACE s_axilite port=dimension bundle=control
#pragma HLS INTERFACE s_axilite port=defectedN bundle=control
#pragma HLS INTERFACE s_axilite port=pairsTest bundle=control
#pragma HLS INTERFACE s_axilite port=pairsTestCount bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
	hlsint i;
	hlsint d = dimension;
	PairDNode pairs[2 * MAXELEMENTS];
	hlsint pairsCount = 0;
	//	TouchesExpansion touchesForExpansion[MAXELEMENTS];
	//	hlsint touchesForExpansionCount = 0;
	hlsint startDefectedNodes[MAXDEFECTEDNODES] = { 0 };
#pragma HLS array_partition variable= startDefectedNodes type=complete
	defectedNodesCount_1 = nDefNodes;
writeDefectedNodesFromMem:for (i = 0; i < defectedNodesCount_1; i++)
{
#pragma HLS PIPELINE II=1
	startDefectedNodes[i] = defectedN[i];
}
if (defectedNodesCount_1 == 1)
{
	pairs[pairsCount].node1 = startDefectedNodes[0];
	if (startDefectedNodes[0] % (d + 1) * 2 < d) {
		pairs[pairsCount].node2 = (startDefectedNodes[0] / (d + 1)) * (d + 1);
	}
	else
	{
		pairs[pairsCount].node2 = (startDefectedNodes[0] / (d + 1)) * (d + 1) + d;
	}
	pairsCount++;
}
else if (defectedNodesCount_1 == 2)
{
	hlsint distance = 0;
	if (startDefectedNodes[0] % (d + 1) * 2 < d) {
		distance += startDefectedNodes[0] % (d + 1);
	}
	else
	{
		distance += d - startDefectedNodes[0] % (d + 1);
	}
	if (startDefectedNodes[1] % (d + 1) * 2 < d) {
		distance += startDefectedNodes[1] % (d + 1);
	}
	else
	{
		distance += d - startDefectedNodes[1] % (d + 1);
	}
	if ((((startDefectedNodes[0] % (d + 1)) > (startDefectedNodes[1] % (d + 1))) && (startDefectedNodes[0] % (d + 1) - startDefectedNodes[1] % (d + 1) + startDefectedNodes[1] / (d + 1) - startDefectedNodes[0] / (d + 1)) > distance) ||
		(((startDefectedNodes[0] % (d + 1)) <= (startDefectedNodes[1] % (d + 1))) && (startDefectedNodes[1] % (d + 1) - startDefectedNodes[0] % (d + 1) + startDefectedNodes[1] / (d + 1) - startDefectedNodes[0] / (d + 1)) > distance))
	{
		for (i = 0; i <= 1; i++) {
			pairs[pairsCount].node1 = startDefectedNodes[i];
			if (startDefectedNodes[i] % (d + 1) * 2 < d) {
				pairs[pairsCount].node2 = (startDefectedNodes[i] / (d + 1)) * (d + 1);
			}
			else
			{
				pairs[pairsCount].node2 = (startDefectedNodes[i] / (d + 1)) * (d + 1) + d;
			}
			pairsCount++;
		}
	}
	else {
		pairs[0].node1 = startDefectedNodes[0];
		pairs[0].node2 = startDefectedNodes[1];
		pairsCount++;
	}
}
else {
	hlsbool finished = false;
	error = -1;
	numberOfEdges_1 = d * d + (d - 1) * (d - 1);
	//initilize the defected nodes
createGraph:
	for (i = 0; i < MAXELEMENTS; i++)
	{
		for (hlsint j = 0; j < MAXELEMENTS + 1; j++)
		{
			graph_1[i][j].number = i * (d + 1) + j;
			graph_1[i][j].weightGot = 0;
			graph_1[i][j].defectedparent = -1;
			graph_1[i][j].y = i;
			graph_1[i][j].x = j;

		//NEED TO DELETE = NULL TO CHECK FOR THE ERROR
			if (j == 0 || j == d)
			{
				graph_1[i][j].isVirtual = true;
			}
			else
			{
				graph_1[i][j].isVirtual = false;
			}
			//initialize the edges
			if (j != d)
			{
				graph_1[i][j].edgesNear[EAST] = i * d + j;
			}
			else
			{
				graph_1[i][j].edgesNear[EAST] = -1;
			}
			if (j != 0)
			{
				graph_1[i][j].edgesNear[WEST] = i * d + j - 1;
			}
			else
			{
				graph_1[i][j].edgesNear[WEST] = -1;
			}
			if (i != d - 1 && j != 0 && j != d)
			{
				graph_1[i][j].edgesNear[SOUTH] = d * d + i * (d - 1) + j - 1;
			}
			else
			{
				graph_1[i][j].edgesNear[SOUTH] = -1;
			}
			if (i != 0 && j != 0 && j != d)
			{
				graph_1[i][j].edgesNear[NORTH] = d * d + (i - 1) * (d - 1) + j - 1;
			}
			else
			{
				graph_1[i][j].edgesNear[NORTH] = -1;
			}

		}
	};
createDefectedNodes:
	for (hlsint h = 0; h < MAXDEFECTEDNODES; h++)
	{

		if (h < defectedNodesCount_1)
		{
			defectedNodes_1[h].x = startDefectedNodes[h] % (d + 1);
			defectedNodes_1[h].y = startDefectedNodes[h] / (d + 1);
			defectedNodes_1[h].nodes[0].x = defectedNodes_1[h].x;
			defectedNodes_1[h].nodes[0].y = defectedNodes_1[h].y;
			defectedNodes_1[h].connectedToVirtual = false;
			defectedNodes_1[h].expandOrReduce = 1;
			defectedNodes_1[h].hasParent = false;
			defectedNodes_1[h].isBlossom = false;
			defectedNodes_1[h].nodesCount = 1;
			defectedNodes_1[h].touchCount = 0;
			defectedNodes_1[h].canTouchCount = 0;
			defectedNodes_1[h].lastRoundModified = 0;
			defectedNodes_1[h].highestParent = -1;
			defectedNodes_1[h].directParent = -1;
			defectedNodes_1[h].myself = h;
			defectedNodes_1[h].usable = true;
			graph_1[defectedNodes_1[h].y][defectedNodes_1[h].x].defectedparent = h;
		}
	}
	//initilize the edges to weight 1000
setEdgeWeight:
	for (hlsint f = 0; f < MAXEDGES; f++)
	{
		edgesWeight_1[f] = MAXWEIGHT;
	}
eternalLoop:
	while (!finished && error == -1)
	{
		actualRound_1++;
	retractionLoop:
		for (i = 0; i < defectedNodesCount_1; i++)
		{
			DefectedNode* defectedNode = &defectedNodes_1[i];

			if (defectedNode->usable && !defectedNode->isBlossom && defectedNode->lastRoundModified < actualRound_1)
			{
				if (!defectedNode->hasParent && defectedNode->expandOrReduce == -1)
				{
					//DEBUG_PRINT(outputFile, "\nReducing defected node %d\n", i);
					processRetractDefectedNodes(d, i);
					defectedNode->lastRoundModified = actualRound_1;
				}
				else if (defectedNode->hasParent && defectedNodes_1[defectedNode->highestParent].expandOrReduce == -1)
				{
					//DEBUG_PRINT(outputFile, "\nReducing defected node %d with parent %d\n", i, defectedNode->highestParent);
					processRetractDefectedNodes(d, i);
					defectedNode->lastRoundModified = actualRound_1;
					if (defectedNodes_1[defectedNode->highestParent].lastRoundModified < actualRound_1)
					{
						defectedNodes_1[defectedNode->highestParent].lastRoundModified = actualRound_1;
						blossoms_1[defectedNodes_1[defectedNode->highestParent].myself].dimension--;
					}
				}
			}
		}
	expantionLoop:
		for (i = 0; i < defectedNodesCount_1; i++)
		{
			DefectedNode* defectedNode = &defectedNodes_1[i];
			if (defectedNode->usable && !defectedNode->isBlossom && defectedNode->lastRoundModified < actualRound_1)
			{
				if (!defectedNode->hasParent && defectedNode->expandOrReduce == 1)
				{
					//DEBUG_PRINT(outputFile, "\n Expanding defected node %d\n", i);
					processExpandDefectedNodes(d, i);//, touchesForExpansion, & touchesForExpansionCount);
					defectedNode->lastRoundModified = actualRound_1;
				}
				else if (defectedNode->hasParent && defectedNodes_1[defectedNode->highestParent].expandOrReduce == 1)
				{
					//DEBUG_PRINT(outputFile, "\nExpanding defected node %d with parent %d\n", i, defectedNode->highestParent);
					processExpandDefectedNodes(d, i);// , touchesForExpansion, & touchesForExpansionCount);
					defectedNode->lastRoundModified = actualRound_1;
					if (defectedNodes_1[defectedNode->highestParent].lastRoundModified < actualRound_1)
					{
						defectedNodes_1[defectedNode->highestParent].lastRoundModified = actualRound_1;
						blossoms_1[defectedNodes_1[defectedNode->highestParent].myself].dimension++;
					}
				}
			}
		}
		if (updated_1)
		{
			if (!roundcheckWithQueue()) //finished if all connected
			{
				//DEBUG_PRINT(outputFile, "All nodes connected\n");
				finished = true;
			}
		}
		updated_1 = false;
	}
	// TODO: Implement the program logic here
	createFinalConnections(d, pairs, &pairsCount);
}
//reset all the counts
blossomsCount_1 = 0;
defectedNodesCount_1 = 0;
numberOfEdges_1 = 0;
actualRound_1 = 0;

(*pairsTestCount) = pairsCount;

setPairLoop:for (i = 0; i < pairsCount; i++) {
pairsTest[i].node1 = (short)pairs[i].node1;
pairsTest[i].node2 = (short)pairs[i].node2;
}
}

