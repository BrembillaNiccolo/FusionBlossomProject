/*
	TODO:
		DONE - when a node expand and touch a node that is the starting point of an INfected one, check in all directy if weight is 0, if so touch between the node that is expanding and the other one
		DONE - delete weak touches if I create a strong touch
		- if strong strong in one part need expansion through the can touches taht becomes weak touches need to see maximum expansion
		DONE - creation of the blossom
		DONE - each modification if from WEAK to STRONG, need to see if a blossom if yes need to modify internal structures of all blossoms
		- disruption of the blossom (if 3 is easy else division even / odd)
		- make the final solution
	DONE:
		- function to delete weak touches -> need tests
		- function to see if needed a blossom -> need to add an array maintaining the nodes ( or do another function)
		- function to create a blossom -> need tests
		- function to modify internal connections of a blossom -> need tests
		- function to make matches for final solution -> need lot of tests
*/

#include <stdio.h>
#include <stdbool.h>
#include "ap_int.h"
#define BITACCURACY 9
#define BOOLACCURACY 1

typedef ap_int<BITACCURACY> hlsint;
typedef ap_uint<BOOLACCURACY> hlsbool;
#define MAXELEMENTS 9
#define NORTH 0
#define EAST 2
#define SOUTH 3
#define WEST 1
#define MAXWEIGHT 1000
#define STRONG 1
#define WEAK 0
#define MAXTOUCHES 2
#define MAX_STACK_SIZE 36
#define MAX_QUEUE_SIZE 36
#define DEBUG -1
#define MAXDEFECTEDNODES 2*MAXELEMENTS //3*MAXELEMENTS
#define MAXBLOSSOMS (MAXELEMENTS+1)/2 //MAXELEMENTS
#define input "/home/users/niccolo.brembilla/Desktop/NewFusion/GlobVar/FusionBlossom/in3d.txt"


#if  defined(DEBUG) && DEBUG > -1
#define DEBUG_ALWAYS_PRINT printf
#else
#define DEBUG_ALWAYS_PRINT
#endif

#if  defined(DEBUG) && DEBUG > 1
#define DEBUG_PRINT printf
#else
#define DEBUG_PRINT
#endif

#if  defined(DEBUG) && DEBUG > 0
#define DEBUG_PRIORITY_PRINT printf
#else
#define DEBUG_PRIORITY_PRINT
#endif



typedef struct node Node;
typedef struct defectedNode DefectedNode;
typedef struct blossom Blossom;
typedef struct edge Edge;

typedef struct pairDefectedNode
{
	short node1;
	short node2;
} PairDefectedNode;

// I don't need the end node pointers because i can find them from the graph
//if edge NORTH then the end node is (x,y-1)
//if edge EAST then the end node is (x+1,y)
//if edge SOUTH then the end node is (x,y+1)
//if edge WEST then the end node is (x-1,y)
typedef struct edge
{
	short number;
	short weight;
	short lastRoundModified;
};

typedef struct node
{
	short number;
	short x;
	short y;
	short weightGot;
	bool isVirtual;
	DefectedNode* defectedparent;
	DefectedNode* blossomParent;
	//the edges are north, east, south, west so the dimension is 4
	Edge* edges[4];
};

typedef struct touch
{
	DefectedNode* node1;
	DefectedNode* node2;
} Touch;

typedef struct canTouchDefectedNode
{
	DefectedNode* node;
	DefectedNode* mychild;
	DefectedNode* otherchild;
} CanTouchDefectedNode;

typedef struct touchDefectedNode
{
	DefectedNode* node;
	DefectedNode* mychild;
	DefectedNode* otherchild;
	//0 for weak, 1 for strong
	bool touchType;
} TouchDefectedNode;

typedef struct defectedNode
{
	//Can possibly add an highest parent to reduce the number of searches

	//NULL if don't have a parent blossom
	short x;
	short y;
	short nodesCount;
	short number;
	short canTouchCount;
	short lastRoundModified;
	short touchCount;
	short expandOrReduce;
	bool hasParent;
	bool isBlossom;
	bool connectedToVirtual;
	bool usable;

	Blossom* highestParent;
	Blossom* directParent;
	//NULL if not blossom
	Blossom* myself;
	// 1 for expand, -1 for reduce
	Node* nodes[5 * MAXELEMENTS];

	TouchDefectedNode touch[MAXTOUCHES];
	CanTouchDefectedNode canTouch[3 * MAXELEMENTS];// max at MAXELEMENTS*MAXELEMENTS, should be 2*MAXELEMENTS-2

};

typedef	struct blossom
{
	short childrenBlossomCount;
	short childrenDefectedNodesCount;
	short needToBeExpanded;
	short dimension;
	bool connectedToVirtual;
	Blossom* parent;
	Blossom* childrenBlossoms[MAXELEMENTS];
	DefectedNode* childrenDefectedNodes[MAXELEMENTS];
	//initializing the dimension to 0
	//+1 if expand, -1 if reduce
	//if 0 at any point after the initialization, then the blossom is not dismanted
	DefectedNode* myself;
};

typedef struct {
	DefectedNode* nodes[MAX_QUEUE_SIZE];
	int front;
	int rear;
} Queue;

typedef struct {
	Blossom* nodes[MAX_QUEUE_SIZE];
	int front;
	int rear;
} BlossomQueue;

void resolve(hlsint nDefNodes, hlsint d, hlsint defectedN[], PairDefectedNode pairs[], hlsint* pairsCount);

//the graph is represented as an adjacency matrix
//the total number of nodes is n * (n-1)
//if j=0 or j=MAXELEMENTS, then the node is a virtual node
// and do not need to be expanded
// to position defected nodes do the following:
//		to find x position do n%d+1
//		to find y position do n/d
Node graph[MAXELEMENTS][MAXELEMENTS + 1];

//THE EDGES ARE NORTH, EAST, SOUTH, WEST
//if x=0 only east
//if x=d+1 only west
//if y=0 only east, south, west
//if y=d only north, east, west  
Edge edges[2 * (MAXELEMENTS * MAXELEMENTS)];

short numberOfEdges = 0;

//Usually only 1% to 5% of the nodes are defected
// need to add for possible blossoms
DefectedNode defectedNodes[MAXDEFECTEDNODES];


short defectedNodesCount = 0;
short firstDefectedNodesCount = 0;

Blossom blossoms[MAXBLOSSOMS];

short blossomsCount = 0;

short actualRound = 0;

void initializeBlossomQueueTestbench(BlossomQueue* queue)
{
	queue->front = -1;
	queue->rear = -1;
}

bool isBlossomQueueEmptyTestbench(BlossomQueue* queue)
{
	return queue->front == -1;
}

bool isBlossomQueueFullTestbench(BlossomQueue* queue)
{
	return (queue->rear + 1) % MAX_QUEUE_SIZE == queue->front;
}

void enqueueBlossomTestbench(BlossomQueue* queue, Blossom* blossom)
{
	if (isBlossomQueueFullTestbench(queue))
	{
		//DEBUG_PRINT("Blossom Queue overflow\n");
		return;
	}
	if (isBlossomQueueEmptyTestbench(queue))
	{
		queue->front = 0;
	}
	queue->rear = (queue->rear + 1) % MAX_QUEUE_SIZE;
	queue->nodes[queue->rear] = blossom;
}

Blossom* dequeueBlossomTestbench(BlossomQueue* queue)
{
	if (isBlossomQueueEmptyTestbench(queue))
	{
		//DEBUG_PRINT("Blossom Queue underflow\n");
		return NULL;
	}
	Blossom* blossom = queue->nodes[queue->front];
	if (queue->front == queue->rear)
	{
		queue->front = -1;
		queue->rear = -1;
	}
	else {
		queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;
	}
	return blossom;
}

void initializeQueueTestbench(Queue* queue)
{
	queue->front = -1;
	queue->rear = -1;
}

bool isQueueEmptyTestbench(Queue* queue)
{
	return queue->front == -1;
}

bool isQueueFullTestbench(Queue* queue)
{
	return (queue->rear + 1) % MAX_QUEUE_SIZE == queue->front;
}

void enqueueTestbench(Queue* queue, DefectedNode* node)
{
	if (isQueueFullTestbench(queue))
	{
		//DEBUG_PRINT("Queue overflow\n");
		return;
	}
	if (isQueueEmptyTestbench(queue))
	{
		queue->front = 0;
	}
	queue->rear = (queue->rear + 1) % MAX_QUEUE_SIZE;
	queue->nodes[queue->rear] = node;
}

DefectedNode* dequeueTestbench(Queue* queue)
{
	if (isQueueEmptyTestbench(queue))
	{
		//DEBUG_PRINT("Queue underflow\n");
		return NULL;
	}
	DefectedNode* node = queue->nodes[queue->front];
	if (queue->front == queue->rear)
	{
		queue->front = -1;
		queue->rear = -1;
	}
	else {
		queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;
	}
	return node;
}

//check if the defected nodes are valid
//d must be even
//d must be less than MAXELEMENTS
//the virtual nodes can't be defected
bool checkNodesTestbench(short* defectedN, short d)
{
	if (d > MAXELEMENTS)
		return false;
	if (d % 2 == 0)
		return false;
	for (short i = 0; i < defectedNodesCount; i++)
	{
		if (defectedN[i] > d * (d + 1))
			return false;
		if (defectedN[i] >= (d * (d + 1)))
			return false;
		if (defectedN[i] % (d + 1) == d || defectedN[i] % (d + 1) == 0)
			return false;
	}
	return true;
}
void modifyOnlyMyInternalConnectionsTestbench(DefectedNode* topBlossomFather, DefectedNode* child) {
	//DEBUG_PRINT("Modifying internal connections for blossomFather %d and child %d\n", topBlossomFather->number, child->number);
	// Modify the touch of the child to have both outgoing connections as weak
	// Start going through the chain and modify 1 strong and 1 weak connection until it returns to the child
	DefectedNode* nextNode = NULL;
	DefectedNode* lastNode = NULL;
	DefectedNode* currentNode = NULL;

	if (child->touch[0].touchType == WEAK && child->touch[1].touchType == WEAK)
	{
		//do nothing
		DEBUG_PRINT("Both connections of the child are weak no modification needed\n");

	}
	else {
		Queue externalQueue;
		Queue externalQueueChild;
		Queue queue;
		initializeQueueTestbench(&externalQueue);
		initializeQueueTestbench(&externalQueueChild);
		//DEBUG_PRINT("Added node %d to the external queue\n", topBlossomFather->number);
		enqueueTestbench(&externalQueue, topBlossomFather);
		enqueueTestbench(&externalQueueChild, child);
		while (!isQueueEmptyTestbench(&externalQueue))
		{
			DefectedNode* currentExternalNode = dequeueTestbench(&externalQueue);
			DefectedNode* currentChildNode = dequeueTestbench(&externalQueueChild);
			do
			{
				lastNode = currentChildNode;
				// if both weak all after are correct
				if (currentChildNode->touch[0].touchType != WEAK || currentChildNode->touch[1].touchType != WEAK)
				{
					//change touch mine and others to weak
					for (short i = 0; i < currentChildNode->touchCount; i++)
					{
						currentChildNode->touch[i].touchType = WEAK;
						//find the corresponding touch in the other node
						for (short j = 0; j < currentChildNode->touch[i].node->touchCount; j++)
						{
							if (currentChildNode->touch[i].node->touch[j].node == currentChildNode)
							{
								currentChildNode->touch[i].node->touch[j].touchType = WEAK;
								break;
							}
						}
						//DEBUG_PRINT("Modified touch between %d and %d to be weak\n", currentChildNode->number, currentChildNode->touch[i].node->number);
					}

					initializeQueueTestbench(&queue);
					//DEBUG_PRINT("Added node %d to the queue\n", currentChildNode->touch[0].node->number);
					enqueueTestbench(&queue, currentChildNode->touch[0].node);

					while (!isQueueEmptyTestbench(&queue))
					{
						currentNode = dequeueTestbench(&queue);
						//DEBUG_PRINT("Processing node %d with touchCount %d\n", currentNode->number, currentNode->touchCount);
						for (short i = 0; i < currentNode->touchCount; i++)
						{
							nextNode = currentNode->touch[i].node;
							//DEBUG_PRINT("Processing touch between %d and %d\n", currentNode->number, nextNode->number);
							if (nextNode != currentChildNode && nextNode != lastNode)
							{
								if (currentNode->touch[i].touchType == STRONG)
								{
									currentNode->touch[i].touchType = WEAK;
									//find the corresponding touch in the next node
									//DEBUG_PRINT("Modified touch between %d and %d to be weak\n", currentNode->number, nextNode->number);
								}
								else {
									currentNode->touch[i].touchType = STRONG;

									//find the corresponding touch in the next node

									//DEBUG_PRINT("Modified touch between %d and %d to be strong\n", currentNode->number, nextNode->number);
								}

								for (short j = 0; j < nextNode->touchCount; j++)
								{
									if (nextNode->touch[j].node == currentNode)
									{
										nextNode->touch[j].touchType = currentNode->touch[i].touchType;
										if (nextNode->touch[j].touchType == STRONG && nextNode->isBlossom)
										{
											enqueueTestbench(&externalQueue, nextNode);
											DefectedNode* newChild = nextNode->touch[j].mychild;
											//find top parent until nextNode
											/*while (newChild->directParent->myself != nextNode)
											{
												newChild = newChild->directParent->myself;
											}
											*/
											enqueueTestbench(&externalQueueChild, newChild);
										}
										break;
									}
								}
								//DEBUG_PRINT("Added node %d to the queue\n", nextNode->number);
								enqueueTestbench(&queue, nextNode);
							}
						}
						lastNode = currentNode;
					}
				}
				currentChildNode = currentChildNode->directParent->myself;

			} while (currentChildNode != currentExternalNode);
		}
	}
}

void modifyInternalConnectionsTestbench(DefectedNode* blossomFather, DefectedNode* child)
{
	//DEBUG_PRINT("Modifying internal connections for blossomFather %d and child %d\n", blossomFather->number, child->number);
	// Modify the touch of the child to have both outgoing connections as weak
	// Start going through the chain and modify 1 strong and 1 weak connection until it returns to the child
	DefectedNode* parent = blossomFather;
	DefectedNode* currentNode = child;
	DefectedNode* nextNode = NULL;
	DefectedNode* lastNode = child;
	bool hasFather = true;

	while (hasFather)
	{
		if (child->touch[0].touchType == WEAK && child->touch[1].touchType == WEAK)
		{
			//do nothing
			DEBUG_PRINT("Both connections of the child %d with father %d are weak no modification needed\n", child->number, parent->number);
		}
		else {
			lastNode = child;
			for (short i = 0; i < child->touchCount; i++)
			{
				child->touch[i].touchType = WEAK;
				//find the corresponding touch in the other node
				for (short j = 0; j < child->touch[i].node->touchCount; j++)
				{
					if (child->touch[i].node->touch[j].node == child)
					{
						child->touch[i].node->touch[j].touchType = WEAK;
						break;
					}
				}
				//DEBUG_PRINT("Modified touch between %d and %d to be weak\n", child->number, child->touch[i].node->number);
			}
			/*
			currentNode = child->touch[0].node;
			short position = -1;

			while (currentNode != NULL)
			{
				//DEBUG_PRINT("Processing node %d with touchCount %d\n", currentNode->number, currentNode->touchCount);
				nextNode = currentNode->touch[0].node;
				if (nextNode != child && nextNode != lastNode) {
					//DEBUG_PRINT("Position 0 , Child is %d, lastNode is %d, nextNode is %d\n", child->number, lastNode->number, nextNode->number);
					position = 0;
				}
				else //always 2 touched inside a blossom
				{
					nextNode = currentNode->touch[1].node;
					if (nextNode != child && nextNode != lastNode)
					{
						//DEBUG_PRINT("Position 1 , Child is %d, lastNode is %d, nextNode is %d\n", child->number, lastNode->number, nextNode->number);
						position = 1;
					}
				}
				lastNode = currentNode;
				if (position != -1) {
					if (currentNode->touch[position].touchType == STRONG)
					{
						currentNode->touch[position].touchType = WEAK;
						//find the corresponding touch in the next node
						//DEBUG_PRINT("Modified touch between %d and %d to be weak\n", currentNode->number, nextNode->number);
					}
					else {
						currentNode->touch[position].touchType = STRONG;
						//find the corresponding touch in the next node

						//DEBUG_PRINT("Modified touch between %d and %d to be strong\n", currentNode->number, nextNode->number);
					}
					for (short j = 0; j < nextNode->touchCount; j++)
					{
						if (nextNode->touch[j].node == currentNode)
						{
							nextNode->touch[j].touchType = currentNode->touch[position].touchType;
							break;
						}
					}
					currentNode = nextNode;
				}
				else {
					currentNode = NULL;
				}
				position = -1;
			}
			*/
			Queue queue;
			initializeQueueTestbench(&queue);
			//DEBUG_PRINT("Added node %d to the queue\n", child->touch[0].node->number);
			enqueueTestbench(&queue, child->touch[0].node);

			while (!isQueueEmptyTestbench(&queue))
			{
				currentNode = dequeueTestbench(&queue);
				//DEBUG_PRINT("Processing node %d with touchCount %d\n", currentNode->number, currentNode->touchCount);
				for (short i = 0; i < currentNode->touchCount; i++)
				{
					nextNode = currentNode->touch[i].node;
					//DEBUG_PRINT("Processing touch between %d and %d\n", currentNode->number, nextNode->number);
					if (nextNode != child && nextNode != lastNode)
					{
						if (currentNode->touch[i].touchType == STRONG)
						{
							currentNode->touch[i].touchType = WEAK;
							for (short j = 0; j < nextNode->touchCount; j++)
							{
								if (nextNode->touch[j].node == currentNode)
								{
									nextNode->touch[j].touchType = currentNode->touch[i].touchType;
									break;
								}
							}
							//find the corresponding touch in the next node
							//DEBUG_PRINT("Modified touch between %d and %d to be weak\n", currentNode->number, nextNode->number);
						}
						else {
							currentNode->touch[i].touchType = STRONG;
							if (currentNode->isBlossom) {
								DefectedNode* newChild = currentNode->touch[i].mychild;
								/*
								//find top parent until currentNode
								while (newChild->directParent->myself != currentNode)
								{
									newChild = newChild->directParent->myself;
								}
								*/
								modifyOnlyMyInternalConnectionsTestbench(currentNode, newChild);
							}
							//find the corresponding touch in the next node
							for (short j = 0; j < nextNode->touchCount; j++)
							{
								if (nextNode->touch[j].node == currentNode)
								{
									nextNode->touch[j].touchType = currentNode->touch[i].touchType;
									if (nextNode->isBlossom) {
										DefectedNode* newChild = nextNode->touch[j].mychild;
										/*
										//find top parent until nextNode
										while (newChild->directParent->myself != nextNode)
										{
											newChild = newChild->directParent->myself;
										}
										*/
										modifyOnlyMyInternalConnectionsTestbench(nextNode, newChild);
									}
									break;
								}
							}

							//DEBUG_PRINT("Modified touch between %d and %d to be strong\n", currentNode->number, nextNode->number);
						}



						//DEBUG_PRINT("Added node %d to the queue\n", nextNode->number);
						enqueueTestbench(&queue, nextNode);
					}
				}
				lastNode = currentNode;
			}

		}



		hasFather = parent->hasParent;
		if (hasFather)
		{
			child = parent;
			parent = parent->directParent->myself;
		}
	}
}
//return 0 if no shift in the first node has been done, 1 if shift in the first node
bool deleteCanTouchTestbench(DefectedNode* node1, DefectedNode* node2)
{
	short i;
	// Find the canTouch in node1
	short canTouchIndex = -1;
	bool shift = 0;
	for ( i = 0; i < node1->canTouchCount; i++)
	{
		if (node1->canTouch[i].node == node2)
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
		if (canTouchIndex != node1->canTouchCount - 1)
		{
			shift = 1;
			for ( i = canTouchIndex; i < node1->canTouchCount - 1; i++)
			{
				node1->canTouch[i] = node1->canTouch[i + 1];
			}
		}
		node1->canTouchCount--;
	}
	// Also do the same for node2
	canTouchIndex = -1;
	for ( i = 0; i < node2->canTouchCount; i++)
	{
		if (node2->canTouch[i].node == node1)
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
		if (canTouchIndex != node2->canTouchCount - 1)
		{
			for ( i = canTouchIndex; i < node2->canTouchCount - 1; i++)
			{
				node2->canTouch[i] = node2->canTouch[i + 1];
			}
		}
		node2->canTouchCount--;
	}
	return shift;
}
void changeAllInChainTestbench(DefectedNode* startNode)
{
	//DEBUG_PRINT("Changing all in chain of %d\n", startNode->number);
	Queue queue;
	DefectedNode* lastNode = startNode;
	short i,j;
	initializeQueueTestbench(&queue);
	enqueueTestbench(&queue, startNode);
	while (!isQueueEmptyTestbench(&queue))
	{
		DefectedNode* currentNode = dequeueTestbench(&queue);
		for ( i = 0; i < currentNode->touchCount; i++)
		{
			DefectedNode* nextNode = currentNode->touch[i].node;
			// Ignore the return touch of the next node that is the same as the previous
			if (nextNode != lastNode)
			{
				if (currentNode->touch[i].touchType == STRONG)
				{
					//need to change for both nodes
					currentNode->touch[i].touchType = WEAK;
					for ( j = 0; j < nextNode->touchCount; j++)
					{
						if (nextNode->touch[j].node == currentNode)
						{
							nextNode->touch[j].touchType = WEAK;
							break;
						}
					}
				}
				else
				{
					currentNode->touch[i].touchType = STRONG;
					for ( j = 0; j < nextNode->touchCount; j++)
					{
						if (nextNode->touch[j].node == currentNode)
						{
							nextNode->touch[j].touchType = STRONG;
							break;
						}
					}
					//if current node or next node is a blossom then need to modify the internal connections
					if (currentNode->isBlossom || nextNode->isBlossom)
					{
						//if the current node is a blossom then the child is the next node
						if (currentNode->isBlossom)
							modifyInternalConnectionsTestbench(currentNode->touch[i].mychild->directParent->myself, currentNode->touch[i].mychild);
						if (nextNode->isBlossom)
							modifyInternalConnectionsTestbench(currentNode->touch[i].otherchild->directParent->myself, currentNode->touch[i].otherchild);
					}
				}
				//print all what was done
				//DEBUG_PRINT("Connection type between %d and %d is %d\n", currentNode->number, nextNode->number, currentNode->touch[i].touchType);
				//DEBUG_PRINT("Added node %d to the queue\n", nextNode->number);
				enqueueTestbench(&queue, nextNode);
			}
		}
		lastNode = currentNode;
	}
}

DefectedNode* findLastNodeInChainTestbench(DefectedNode* startNode)
{
	//DEBUG_PRINT("Finding last node in chain of %d\n", startNode->number);
	Queue queue;
	DefectedNode* lastNode = startNode;

	initializeQueueTestbench(&queue);
	enqueueTestbench(&queue, startNode);
	while (!isQueueEmptyTestbench(&queue))
	{
		DefectedNode* currentNode = dequeueTestbench(&queue);
		for (short i = 0; i < currentNode->touchCount; i++)
		{
			DefectedNode* nextNode = currentNode->touch[i].node;
			// Ignore the return touch of the next node that is the same as the previous
			if (nextNode != lastNode)
			{
				//DEBUG_PRINT("Added node %d to the queue\n", nextNode->number);
				enqueueTestbench(&queue, nextNode);
			}
		}
		lastNode = currentNode;
	}
	//DEBUG_PRINT("Last node in chain is %d\n", lastNode->number);
	return lastNode;
}

void printAllInChainTestbench(DefectedNode* startNode)
{
	//DEBUG_PRINT("Printing all nodes in chain of %d\n", startNode->number);
	Queue queue;
	DefectedNode* lastNode = startNode;

	initializeQueueTestbench(&queue);
	enqueueTestbench(&queue, startNode);
	while (!isQueueEmptyTestbench(&queue))
	{
		DefectedNode* currentNode = dequeueTestbench(&queue);
		for (short i = 0; i < currentNode->touchCount; i++)
		{
			DefectedNode* nextNode = currentNode->touch[i].node;
			// Ignore the return touch of the next node that is the same as the previous
			if (nextNode != lastNode)
			{
				//DEBUG_PRINT("Connection type between %d and %d is %d\n", currentNode->number, nextNode->number, currentNode->touch[i].touchType);
				//DEBUG_PRINT("Added node %d to the queue\n", nextNode->number);
				enqueueTestbench(&queue, nextNode);
			}
		}
		lastNode = currentNode;
	}

}
void connectNodesTestbench(DefectedNode* p1, DefectedNode* p2, DefectedNode* child1, DefectedNode* child2, bool type)
{
	p1->touch[p1->touchCount].node = p2;
	p1->touch[p1->touchCount].touchType = type;
	p1->touch[p1->touchCount].otherchild = child2;
	p1->touch[p1->touchCount].mychild = child1;
	p1->touchCount++;
	p2->touch[p2->touchCount].node = p1;
	p2->touch[p2->touchCount].touchType = type;
	p2->touch[p2->touchCount].otherchild = child1;
	p2->touch[p2->touchCount].mychild = child2;
	p2->touchCount++;
}

void connectCanTouchTestbench(DefectedNode* startNode) {
	short lastFather[MAX_QUEUE_SIZE];
	short distance[MAX_QUEUE_SIZE];
	DefectedNode* nodes[MAX_QUEUE_SIZE];
	short nodesToExpand[MAX_QUEUE_SIZE];
	short nodesToExpandCount = 0;
	short newNodesToexpand[MAX_QUEUE_SIZE];
	short newNodesToexpandCount = 0;
	short nodeCount = 0;
	short dist = 0;
	short father = 0;
	bool expandable = true;
	short i;
	DefectedNode* currentNode = startNode;
	nodes[0] = startNode;
	lastFather[0] = -1;
	distance[0] = 0;
	nodeCount++;
	currentNode = findLastNodeInChainTestbench(startNode);
	nodes[nodeCount] = currentNode;
	lastFather[nodeCount] = 0;
	distance[nodeCount] = 0;
	nodeCount++;
	nodesToExpand[nodesToExpandCount] = 1;
	nodesToExpandCount++;
	if (currentNode != startNode->touch[0].node)
	{
		printf("ERROR!!");
	}
	while (expandable) {
		dist++;
		expandable = false;
		for ( i = 0; i < nodesToExpandCount; i++)
		{
			father = nodesToExpand[i];
			currentNode = nodes[father];
			//DEBUG_PRINT("Processing node %d with distance %d\n", currentNode->number, distance[father]);
			if (currentNode->canTouchCount > 0)
			{
				for (short j = currentNode->canTouchCount - 1; j >= 0; j--)
				{
					DefectedNode* nextNode = currentNode->canTouch[j].node;
					if (nextNode->touchCount != MAXTOUCHES)
					{
						bool found = false;
						for (short k = 0; k < nodeCount; k++)
						{
							if (nodes[k] == nextNode)
							{
								found = true;
								break;
							}
						}
						if (!found)
						{
							nodes[nodeCount] = nextNode;
							lastFather[nodeCount] = father;
							distance[nodeCount] = dist;
							nodeCount++;
							//DEBUG_PRINT("Added node %d to the queue\n", nextNode->number);
							DefectedNode* afterNode = findLastNodeInChainTestbench(nextNode);
							nodes[nodeCount] = afterNode;
							lastFather[nodeCount] = nodeCount - 1;
							distance[nodeCount] = dist;
							nodeCount++;
							if (afterNode != nextNode->touch[0].node)
							{
								printf("ERROR!!");
							}
							//DEBUG_PRINT("Added node %d to the queue\n", afterNode->number);

							if (afterNode->canTouchCount > 0)
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
		for ( i = 0; i < newNodesToexpandCount; i++)
		{
			nodesToExpand[i] = newNodesToexpand[i];
		}
		nodesToExpandCount = newNodesToexpandCount;
		newNodesToexpandCount = 0;
	}
	short maxDistance = -1;
	short lastNode = -1;
	for ( i = nodeCount - 1; i >= 0; i--)
	{
		//DEBUG_PRINT("Node %d has distance %d\n", nodes[i]->number, distance[i]);
		if (distance[i] > maxDistance)
		{
			maxDistance = distance[i];
			lastNode = i;
		}
	}
	short nextToConnect;
	while (lastNode != -1)
	{
		//DEBUG_PRINT("Last Node in chain %d has distance %d\n", nodes[lastNode]->number, distance[lastNode]);
		lastNode = lastFather[lastNode];
		//DEBUG_PRINT("First Node in chain %d has distance %d\n", nodes[lastNode]->number, distance[lastNode]);
		nextToConnect = lastNode;
		lastNode = lastFather[lastNode];
		if (lastNode != -1)
		{
			bool noTouch = true;
			//DEBUG_PRINT("Connecting %d to %d with weak Connection\n", nodes[lastNode]->number, nodes[nextToConnect]->number);
			for ( i = 0; i < nodes[lastNode]->canTouchCount; i++)
			{
				if (nodes[lastNode]->canTouch[i].node == nodes[nextToConnect])
				{
					noTouch = false;
					if (nodes[lastNode]->touchCount == MAXTOUCHES || nodes[nextToConnect]->touchCount == MAXTOUCHES)
					{
						printf("ERROR!!");
					}
					else {
						connectNodesTestbench(nodes[lastNode], nodes[nextToConnect], nodes[lastNode]->canTouch[i].mychild, nodes[lastNode]->canTouch[i].otherchild, WEAK);
						deleteCanTouchTestbench(nodes[lastNode], nodes[nextToConnect]);
					}
				}
			}
			if (noTouch)
			{
				printf("ERROR!!");
			}
		}
	}
}

void deleteTouchTestbench(DefectedNode* node1, DefectedNode* node2, bool completely)
{
	short i;
	//DEBUG_PRINT("Deleting touch between %d and %d\n", node1->number, node2->number);
	// Find the touch in node1
	short touchIndex = -1;

	for ( i = 0; i < node1->touchCount; i++)
	{
		if (node1->touch[i].node == node2)
		{
			touchIndex = i;
			break;
		}
	}
	// If touch is found, delete it
	if (touchIndex != -1)
	{
		if (!completely)
		{
			node1->canTouch[node1->canTouchCount].node = node2;
			node1->canTouch[node1->canTouchCount].mychild = node1->touch[touchIndex].mychild;
			node1->canTouch[node1->canTouchCount].otherchild = node1->touch[touchIndex].otherchild;
			node1->canTouchCount++;
		}
		// Update the values
		//if last element then no need to move
		//else move the last element to the position of the deleted element
		// so node is in 0 and need to move 1 to 0
		if (touchIndex != node1->touchCount - 1)
			node1->touch[0] = node1->touch[node1->touchCount - 1];
		node1->touchCount--;
	}
	//also do for node2
	touchIndex = -1;
	for ( i = 0; i < node2->touchCount; i++)
	{
		if (node2->touch[i].node == node1)
		{
			touchIndex = i;
			break;
		}
	}
	// If touch is found, delete it
	if (touchIndex != -1)
	{
		// Move the eliminated touch to canTouch
		if (!completely)
		{
			node2->canTouch[node2->canTouchCount].node = node1;
			node2->canTouch[node2->canTouchCount].mychild = node2->touch[touchIndex].mychild;
			node2->canTouch[node2->canTouchCount].otherchild = node2->touch[touchIndex].otherchild;
			node2->canTouchCount++;
		}
		// Update the values
		//if last element then no need to move
		//else move the last element to the position of the deleted element
		// so node is in 0 and need to move 1 to 0
		if (touchIndex != node2->touchCount - 1)
			node2->touch[0] = node2->touch[node2->touchCount - 1];
		node2->touchCount--;

	}
}

//TODO need to return the element in the possible blossom
int checkForLoopTestbench(DefectedNode* node1, DefectedNode* node2)
{
	//DEBUG_PRINT("Checking for loop\n");
	// node1 is the last node of the chain
	Queue queue;
	DefectedNode* lastNode = node1;
	short nodesCount = 0;

	initializeQueueTestbench(&queue);
	enqueueTestbench(&queue, node1);
	while (!isQueueEmptyTestbench(&queue))
	{
		nodesCount++;
		DefectedNode* currentNode = dequeueTestbench(&queue);
		//DEBUG_PRINT("Node %d has touch Count %d\n", currentNode->number, currentNode->touchCount);
		for (short i = 0; i < currentNode->touchCount; i++)
		{
			DefectedNode* nextNode = currentNode->touch[i].node;
			// if next node is the first node in the chain, then there is a loop exit the for and the while
			if (nextNode == node2)
			{
				nodesCount++;
				//DEBUG_PRINT("Loop found between %d and %d with dimension %d\n", node1->number, nextNode->number, nodesCount);
				return nodesCount;
			}
			// Ignore the return touch of the next node that is the same as the previous
			if (nextNode != lastNode)
			{
				//DEBUG_PRINT("Connection type between %d and %d is %d\n", currentNode->number, nextNode->number, currentNode->touch[i].touchType);
				//DEBUG_PRINT("Added node %d to the queue\n", nextNode->number);
				enqueueTestbench(&queue, nextNode);
			}
		}
		lastNode = currentNode;
	}
	//check if there is a loop
	return 0;
}

int checkIntraForLoopTestbench(DefectedNode* node1, DefectedNode* node2)
{
	short i;
	//DEBUG_PRINT("Checking for Intra loop\n");
	Queue queue;
	DefectedNode* lastNode = node1;

	initializeQueueTestbench(&queue);
	enqueueTestbench(&queue, node1->touch[0].node);
	while (!isQueueEmptyTestbench(&queue))
	{
		DefectedNode* currentNode = dequeueTestbench(&queue);
		for ( i = 0; i < currentNode->touchCount; i++)
		{
			DefectedNode* nextNode = currentNode->touch[i].node;
			// Ignore the return touch of the next node that is the same as the previous
			if (nextNode != lastNode)
			{
				enqueueTestbench(&queue, nextNode);
			}
		}
		lastNode = currentNode;
	}

	//DEBUG_PRINT("Last node in chain is %d\n", lastNode->number);
	initializeQueueTestbench(&queue);
	enqueueTestbench(&queue, lastNode);
	short nodesCount = 0;
	bool foundFirst = 0;
	short whichFound = -1;
	while (!isQueueEmptyTestbench(&queue))
	{
		DefectedNode* currentNode = dequeueTestbench(&queue);
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
		//DEBUG_PRINT("Node in count %d is %d\n", nodesCount, currentNode->number);
		for ( i = 0; i < currentNode->touchCount; i++)
		{
			DefectedNode* nextNode = currentNode->touch[i].node;
			// Ignore the return touch of the next node that is the same as the previous
			if (whichFound == 1 && nextNode == node2)
			{
				//DEBUG_PRINT("Loop ----------------- found between %d and %d with dimension %d\n", currentNode->number, nextNode->number, nodesCount);
				return nodesCount;
			}
			else if (whichFound == 2 && nextNode == node1)
			{
				//DEBUG_PRINT("Loop --------------------- found between %d and %d with dimension %d\n", currentNode->number, nextNode->number, nodesCount);
				return nodesCount;
			}
			if (nextNode != lastNode)
			{
				//DEBUG_PRINT("Connection type between %d and %d is %d\n", currentNode->number, nextNode->number, currentNode->touch[i].touchType);
				//DEBUG_PRINT("Added node %d to the queue\n", nextNode->number);
				enqueueTestbench(&queue, nextNode);
			}
		}
		lastNode = currentNode;
	}
	//check if there is a loop
	return 0;
}


void getDefectedForLoopTestbench(DefectedNode* startNode, DefectedNode* endNode, DefectedNode* nodesInLoop[], short* nodesInLoopCount)
{
	//DEBUG_PRINT("Getting defected nodes in loop\n");
	Queue queue;
	DefectedNode* lastNode = startNode;

	initializeQueueTestbench(&queue);
	enqueueTestbench(&queue, startNode);
	//DEBUG_PRINT("Added node %d to the queue\n", startNode->number);
	while (!isQueueEmptyTestbench(&queue))
	{

		DefectedNode* currentNode = dequeueTestbench(&queue);
		nodesInLoop[*nodesInLoopCount] = currentNode;
		(*nodesInLoopCount)++;
		for (short i = 0; i < currentNode->touchCount; i++)
		{
			DefectedNode* nextNode = currentNode->touch[i].node;
			// if next node is the first node in the chain, then there is a loop exit the for and the while
			if (nextNode == endNode)
			{
				nodesInLoop[*nodesInLoopCount] = nextNode;
				(*nodesInLoopCount)++;
				//DEBUG_PRINT("Loop found between %d and %d with dimension %d\n", startNode->number, nextNode->number, (*nodesInLoopCount));
				return;
			}
			// Ignore the return touch of the next node that is the same as the previous
			if (nextNode != lastNode)
			{
				//DEBUG_PRINT("Connection type between %d and %d is %d\n", currentNode->number, nextNode->number, currentNode->touch[i].touchType);
				//DEBUG_PRINT("Added node %d to the queue\n", nextNode->number);
				enqueueTestbench(&queue, nextNode);
			}
		}
		lastNode = currentNode;
	}
}

void deleteWeakConnectionsTestbench(DefectedNode* startNode)
{
	Queue queue;
	DefectedNode* lastNode = startNode;
	short i;
	initializeQueueTestbench(&queue);
	enqueueTestbench(&queue, startNode);
	//DEBUG_PRINT("Deleting weak connections\n");
	// Delete the first week connections to be sure don't create a loop
	for ( i = 0; i < startNode->touchCount; i++)
	{
		//DEBUG_PRINT("Node %d has touch %d\n", startNode->number, startNode->touch[i].node->number);
		//delete all weak connections
		if (startNode->touch[i].touchType == WEAK)
		{
			//DEBUG_PRINT("Deleting touch between %d and %d because WEAK\n", startNode->number, startNode->touch[i].node->number);
			deleteTouchTestbench(startNode, startNode->touch[i].node, false);
			i--;
		}
	}
	while (!isQueueEmptyTestbench(&queue))
	{
		DefectedNode* currentNode = dequeueTestbench(&queue);
		//DEBUG_PRINT("Current node is %d\n", currentNode->number);
		for ( i = 0; i < currentNode->touchCount; i++)
		{
			DefectedNode* nextNode = currentNode->touch[i].node;
			// Ignore the return touch of the next node that is the same as the previous
			if (nextNode != lastNode && nextNode != startNode)
			{
				if (currentNode->touch[i].touchType == WEAK)
				{
					//DEBUG_PRINT("Deleting touch between %d and %d because WEAK\n", currentNode->number, nextNode->number);
					// Move the weak touch to canTouch
					deleteTouchTestbench(currentNode, nextNode, false);
					// Decrement i to process the new touch at index i
					i--;
				}
				//DEBUG_PRINT("Added node %d to the queue\n", nextNode->number);
				enqueueTestbench(&queue, nextNode);
			}
		}
		lastNode = currentNode;
	}
	//DEBUG_PRINT("End of deleting weak connections\n");
}

void createBlossomTestbench(DefectedNode* nodesInBlossom[], short nodesInBlossomCount, DefectedNode* node1, DefectedNode* node2, DefectedNode* child1, DefectedNode* child2)
{
	short i,j;
	//DEBUG_PRINT("Creating blossom\n");
	// Create a new blossom
	Blossom* newBlossom = &blossoms[blossomsCount];
	blossomsCount++;
	newBlossom->parent = NULL;
	newBlossom->childrenBlossomCount = 0;
	newBlossom->childrenDefectedNodesCount = 0;
	newBlossom->connectedToVirtual = false;
	newBlossom->needToBeExpanded = 0;
	newBlossom->dimension = 0;
	newBlossom->myself = NULL;

	// Create a new defected node for the blossom
	DefectedNode* newDefectedNode = &defectedNodes[defectedNodesCount];
	defectedNodesCount++;
	newDefectedNode->highestParent = NULL;
	newDefectedNode->directParent = NULL;
	newDefectedNode->hasParent = false;
	newDefectedNode->myself = newBlossom;
	newDefectedNode->isBlossom = true;
	newDefectedNode->x = -1; // Set the x and y coordinates to -1 for the blossom
	newDefectedNode->y = -1;
	newDefectedNode->connectedToVirtual = false;
	newDefectedNode->expandOrReduce = 0;
	newDefectedNode->nodesCount = nodesInBlossomCount;
	newDefectedNode->number = defectedNodesCount - 1;
	newDefectedNode->touchCount = 0;
	newDefectedNode->canTouchCount = 0;
	newDefectedNode->lastRoundModified = actualRound;
	newDefectedNode->usable = true;

	// Link the blossom and the defected node
	newBlossom->myself = newDefectedNode;
	// Link the defected nodes in the blossom to the blossom and update their parent
	for ( i = 0; i < nodesInBlossomCount; i++)
	{
		nodesInBlossom[i]->highestParent = newBlossom;
		if (!nodesInBlossom[i]->hasParent)
			nodesInBlossom[i]->directParent = newBlossom;
		nodesInBlossom[i]->hasParent = true;
		if (!nodesInBlossom[i]->isBlossom)
		{
			newBlossom->childrenDefectedNodes[newBlossom->childrenDefectedNodesCount] = nodesInBlossom[i];
			newBlossom->childrenDefectedNodesCount++;
			for ( j = 0; j < nodesInBlossom[i]->nodesCount; j++)
			{
				if (nodesInBlossom[i]->nodes[j] != NULL && nodesInBlossom[i]->lastRoundModified == actualRound)
				{
					nodesInBlossom[i]->nodes[j]->blossomParent = newBlossom->myself;
					//DEBUG_PRINT("Node %d has blossom parent %d and parent %d\n", nodesInBlossom[i]->nodes[j]->number, nodesInBlossom[i]->nodes[j]->blossomParent->number, nodesInBlossom[i]->nodes[j]->defectedparent->number);
				}
			}
		}
		else
		{
			newBlossom->childrenBlossoms[newBlossom->childrenBlossomCount] = nodesInBlossom[i]->myself;
			newBlossom->childrenBlossomCount++;
			nodesInBlossom[i]->myself->parent = newBlossom;
		}
		if (nodesInBlossom[i]->connectedToVirtual)
			newDefectedNode->connectedToVirtual = true;
	}

	BlossomQueue queue;
	initializeBlossomQueueTestbench(&queue);
	for ( i = 0; i < newBlossom->childrenBlossomCount; i++)
	{
		enqueueBlossomTestbench(&queue, newBlossom->childrenBlossoms[i]);
	}
	// Set the highest parent of all the children of the blossom
	while (!isBlossomQueueEmptyTestbench(&queue))
	{
		Blossom* currentBlossom = dequeueBlossomTestbench(&queue);
		currentBlossom->myself->highestParent = newBlossom;
		//put each blossom children in the queue
		for ( i = 0; i < currentBlossom->childrenBlossomCount; i++)
		{
			enqueueBlossomTestbench(&queue, currentBlossom->childrenBlossoms[i]);
		}
		//put each defectedNode children in defectedNodes
		for ( i = 0; i < currentBlossom->childrenDefectedNodesCount; i++)
		{
			//update highest parent
			currentBlossom->childrenDefectedNodes[i]->highestParent = newBlossom;
			// to all node of the defected the blossom is the blossom parent
			for ( j = 0; j < currentBlossom->childrenDefectedNodes[i]->nodesCount; j++)
			{
				if (currentBlossom->childrenDefectedNodes[i]->nodes[j] != NULL)
				{
					currentBlossom->childrenDefectedNodes[i]->nodes[j]->blossomParent = newBlossom->myself;
					//DEBUG_PRINT("Node %d has blossom parent %d and parent %d\n", currentBlossom->childrenDefectedNodes[i]->nodes[j]->number, currentBlossom->childrenDefectedNodes[i]->nodes[j]->blossomParent->number, currentBlossom->childrenDefectedNodes[i]->nodes[j]->defectedparent->number);
				}
			}
		}

	}

	short number = 0;
	//DEBUG_PRINT("Number of connections of the blossom %d\n", newDefectedNode->touchCount);
	//DEBUG_PRINT("transporting connections\n");
	// Transport the touches of the internal defected nodes to the blossom
	for ( i = 0; i < nodesInBlossomCount; i++)
	{
		DefectedNode* currentNode = nodesInBlossom[i];

		//DEBUG_PRINT("Processing touches of node %d\n", currentNode->number);
		for ( j = 0; j < currentNode->touchCount; j++)
		{
			DefectedNode* nextNode = currentNode->touch[j].node;
			DefectedNode* otherChild = currentNode->touch[j].otherchild;
			DefectedNode* myChild = currentNode->touch[j].mychild;
			//DEBUG_PRINT("Processing touch between %d and %d\n", currentNode->number, nextNode->number);
			if (nextNode->highestParent != newBlossom && number < MAXTOUCHES)
			{
				//DEBUG_PRINT("Deleting connection between %d and %d because not child of the blossom\n", currentNode->number, nextNode->number);
				int type = currentNode->touch[j].touchType;
				//delete the touch completely
				deleteTouchTestbench(currentNode, nextNode, true);
				// Connect the two nodes with a weak connection
				//DEBUG_PRINT(" Connection number %d between %d and %d\n", number, currentNode->number, nextNode->number);
				connectNodesTestbench(newDefectedNode, nextNode, currentNode, otherChild, type);
				j--;
			}
			else if (number >= MAXTOUCHES)
			{
				// because at most the 2 external nodes can have connections with nodes outside the blossom
				//DEBUG_PRINT("IMPOSSIBLE, MORE THAN 2 CONNECTIONS\n");
				//add connection to canTouch and delete as before
				currentNode->canTouch[currentNode->canTouchCount].node = nextNode;
				currentNode->canTouch[currentNode->canTouchCount].mychild = myChild;
				currentNode->canTouch[currentNode->canTouchCount].otherchild = otherChild;
				currentNode->canTouchCount++;
				nextNode->canTouch[nextNode->canTouchCount].node = currentNode;
				nextNode->canTouch[nextNode->canTouchCount].mychild = otherChild;
				nextNode->canTouch[nextNode->canTouchCount].otherchild = myChild;
				nextNode->canTouchCount++;
				deleteTouchTestbench(currentNode, nextNode, true);
			}
		}
	}

	// Sort the touches of the blossom in ascending order based on the node number
	for ( i = 0; i < newDefectedNode->touchCount - 1; i++)
	{
		for ( j = i + 1; j < newDefectedNode->touchCount; j++)
		{
			if (newDefectedNode->touch[i].node->number > newDefectedNode->touch[j].node->number)
			{
				TouchDefectedNode temp = newDefectedNode->touch[i];
				newDefectedNode->touch[i] = newDefectedNode->touch[j];
				newDefectedNode->touch[j] = temp;
			}
		}
	}
	// Connect the two nodes that still need to be connected with a weak connection
	connectNodesTestbench(node1, node2, child1, child2, WEAK);
	//DEBUG_PRINT("Blossom created\n");
}
// nedd to pass the direct father and the connected node
DefectedNode* getLastChildTestbench(DefectedNode* parent, DefectedNode* child)
{
	while (child->directParent->myself != parent)
	{
		child = child->directParent->myself;
	}
	return child;
}
void touchesProcessingTestbench(DefectedNode* p1, DefectedNode* p2, DefectedNode* child1, DefectedNode* child2)
{
	short i;
	if (child1->isBlossom || child2->isBlossom)
	{
		DEBUG_ALWAYS_PRINT("ERROR, ONE OF THE CHILDREN IS A BLOSSOM\n");
	}
	//DEBUG_PRINT("Processing touches between %d and %d\n", p1->number, p2->number);
	if (p1 == p2)
	{
		//DEBUG_PRINT("Nodes have the same parent\n");
		//if the parent is the same then the children are connected
		return;
	}
	//check if parents already connected or can touch
	//DEBUG_PRINT("Touch count of node %d is %d\n", p1->number, p1->touchCount);
	//DEBUG_PRINT("Touch count of node %d is %d\n", p2->number, p2->touchCount);
	for ( i = 0; i < p1->touchCount; i++)
	{
		if (p1->touch[i].node == p2)
		{
			//if the parent is the same then the children are connected
			//DEBUG_PRINT("Nodes are already connected\n");
			return;
		}
	}
	for ( i = 0; i < p1->canTouchCount; i++)
	{
		//DEBUG_PRINT("NODE %d CAN ALREADY TOUCH %d\n", p1->number, p1->canTouch[i].node->number);
		//if in can touch and one of the 2 cannot get another connection
		if (p1->canTouch[i].node == p2)
		{
			//if the parent is the same then the children are connected
			//DEBUG_PRINT("Nodes can already touch\n");
			return;
		}
	}
	if (p1->touchCount == 0 && p2->touchCount == 0)
	{
		//strong touch
		connectNodesTestbench(p1, p2, child1, child2, STRONG);
		//if one is a blossom then need to modify the internal connections
		if (p1->isBlossom)
			modifyInternalConnectionsTestbench(child1->directParent->myself, child1);
		if (p2->isBlossom)
			modifyInternalConnectionsTestbench(child2->directParent->myself, child2);
		//DEBUG_PRINT("First connection of both nodes\n");
	}
	else if (p1->touchCount == 2 || p2->touchCount == 2)
	{
		//NO IF IT CREATE A LOOP
		int loop;

		if (p1->touchCount == 2)
		{
			if (p2->touchCount == 2) {
				printf("NOTDONE\n");
				loop = checkIntraForLoopTestbench(p2, p1);

			}

			else
				loop = checkForLoopTestbench(p2, p1);
		}
		else
			loop = checkForLoopTestbench(p1, p2);

		if (loop == 0)
		{
			//weak touch		
			p1->canTouch[p1->canTouchCount].node = p2;
			p2->canTouch[p2->canTouchCount].node = p1;
			p1->canTouch[p1->canTouchCount].mychild = child1;
			p2->canTouch[p2->canTouchCount].mychild = child2;
			p1->canTouch[p1->canTouchCount].otherchild = child2;
			p2->canTouch[p2->canTouchCount].otherchild = child1;
			p1->canTouchCount++;
			p2->canTouchCount++;
			//DEBUG_PRINT("One node has already 2 connections, added to canTouch\n");
			//DEBUG_PRINT("Node %d has %d canTouch\n", p1->number, p1->canTouchCount);
			//DEBUG_PRINT("Node %d has %d canTouch\n", p2->number, p2->canTouchCount);
		}
		else {
			if (loop % 2 == 1)
			{

				//CREATE AN ARRAY IN WHICH TO PUT THE NODES THAT ARE IN THE LOOP
				DefectedNode* nodesInLoop[MAXELEMENTS];
				short nodesInLoopCount = 0;
				if (p1->touchCount == 2)
				{
					if (p2->touchCount == 2)
					{
						printf("NOTDONE\n");
						//getDefectedFromIntraForLoop(p2, p1, nodesInLoop, &nodesInLoopCount);
					}
					else
						getDefectedForLoopTestbench(p2, p1, nodesInLoop, &nodesInLoopCount);
				}
				else
					getDefectedForLoopTestbench(p1, p2, nodesInLoop, &nodesInLoopCount);
				//create blossom
				if (nodesInLoopCount > 3) {
					printf("ERROR BLOSSOM NODE WITH MORE THAN 3 nodes\n");
				}
				createBlossomTestbench(nodesInLoop, nodesInLoopCount, p1, p2, child1, child2);
			}
			//else
				//DEBUG_PRINT("CHECK THE LOOP IS EVEN\n");

		}
	}
	else {
		//define all type of interctions that the 2 nodes could have
		DefectedNode* firstNode;
		DefectedNode* secondNode;
		if (p1->touchCount == 0 || p2->touchCount == 0)
		{
			//at least one first connection
			if (p1->touchCount == 0)
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
			if (secondNode->touch[0].touchType == WEAK)
			{
				// NONE -- WEAK 
				// create strong connection
				connectNodesTestbench(p1, p2, child1, child2, STRONG);
				//if one is a blossom then need to modify the internal connections
				if (p1->isBlossom)
					modifyInternalConnectionsTestbench(child1->directParent->myself, child1);
				if (p2->isBlossom)
					modifyInternalConnectionsTestbench(child2->directParent->myself, child2);
				//TODO DELETE ALL WEAK CONNECTIONS THE STRUCTURE IS STABLE ->  PUT IN CAN TOUCH
				deleteWeakConnectionsTestbench(firstNode);
				//DEBUG_PRINT("Node %d, with no connection and Node %d, with weak connection connected with Strong connection\n", firstNode->number, secondNode->number);
			}
			else
			{
				//weak touch
				//need to see the last node in the chain and see it's touch type
				DefectedNode* lastNode = findLastNodeInChainTestbench(secondNode);
				if (lastNode->touch[0].touchType == WEAK)
				{
					// NONE -- STRONG (weak)
					changeAllInChainTestbench(secondNode);
					connectNodesTestbench(p1, p2, child1, child2, STRONG);
					//if one is a blossom then need to modify the internal connections
					if (p1->isBlossom)
						modifyInternalConnectionsTestbench(child1->directParent->myself, child1);
					if (p2->isBlossom)
						modifyInternalConnectionsTestbench(child2->directParent->myself, child2);
					//TODO DELETE ALL WEAK CONNECTIONS THE STRUCTURE IS STABLE ->  PUT IN CAN TOUCH
					deleteWeakConnectionsTestbench(firstNode);
					//DEBUG_PRINT("Node %d, with no connection and Node %d, with Strong(weak) connection connected with Strong connection\n", firstNode->number, secondNode->number);
				}
				else
				{
					//NONE -- STRONG (strong)
					//weak touch
					//TODO CHECK IF CANTOUCH OF LAST NODE IS NOT 0 EXTENDS TO NEXT  MUST FIND THE LONGEST EXTENSION AND THEN CREATE THE CONNECTIONS ( PROBABLY ALL WEAK )
					connectCanTouchTestbench(secondNode);
					connectNodesTestbench(p1, p2, child1, child2, WEAK);
					//DEBUG_PRINT("Node %d, with no connection and Node %d, with Strong(strong) connection connected with weak connection\n", firstNode->number, secondNode->number);
				}

			}
		}
		else
		{
			//nodes connected to 1 other node
			//DEBUG_PRINT("Both touch one node\n");
			//NEED TO CHECK FOR LOOP THAT WILL CREATE A BLOSSOM
			int loop = checkForLoopTestbench(p1, p2);
			if (loop == 0)
			{
				//no loops
				//DEBUG_PRINT("No loops\n");
				if (p1->touch[0].touchType == WEAK || p2->touch[0].touchType == WEAK)
				{
					if (p1->touch[0].touchType == WEAK)
					{
						firstNode = p1;
						secondNode = p2;
					}
					else
					{
						firstNode = p2;
						secondNode = p1;
					}
					if (secondNode->touch[0].touchType == WEAK)
					{
						// WEAK -- WEAK
						// the only way they can expand is ahaving an odd number of nodes in the chain-> the last connection must be Strong 
						// so after the union the chain will have an even number of nodes and the weak connections can be dismantled
						// create strong connection
						connectNodesTestbench(p1, p2, child1, child2, STRONG);
						//if one is a blossom then need to modify the internal connections
						if (p1->isBlossom || p2->isBlossom)
						{
							if (p1->isBlossom)
								modifyInternalConnectionsTestbench(child1->directParent->myself, child1);
							else
								modifyInternalConnectionsTestbench(child2->directParent->myself, child2);
						}
						deleteWeakConnectionsTestbench(firstNode);
						//TODO DELETE ALL WEAK CONNECTIONS THE STRUCTURE IS STABLE ->  PUT IN CAN TOUCH
						//DEBUG_PRINT("Node %d, with weak connection and Node %d, with weak connection connected with Strong connection\n", firstNode->number, secondNode->number);
					}
					else
					{
						//DEBUG_PRINT("---------------------------------------------------");
						DefectedNode* lastNode = findLastNodeInChainTestbench(secondNode);
						if (lastNode->touch[0].touchType == WEAK)
						{
							// WEAK -- STRONG (weak)
							changeAllInChainTestbench(secondNode);
							connectNodesTestbench(p1, p2, child1, child2, STRONG);
							//if one is a blossom then need to modify the internal connections
							if (p1->isBlossom)
								modifyInternalConnectionsTestbench(child1->directParent->myself, child1);
							if (p2->isBlossom)
								modifyInternalConnectionsTestbench(child2->directParent->myself, child2);

							deleteWeakConnectionsTestbench(lastNode);
							//DEBUG_PRINT("Node %d, with weak connection and Node %d, with strong connection (weak) connected with Strong connection\n", firstNode->number, secondNode->number);
						}
						else {
							// WEAK -- STRONG (strong)
							//weak touch
							//TODO CHECK IF CANTOUCH OF LAST NODE IS NOT 0 EXTENDS TO NEXT  MUST FIND THE LONGEST EXTENSION AND THEN CREATE THE CONNECTIONS ( PROBABLY ALL WEAK )
							//NEED FROM STRONG PART TO CHECK IF CAN EXPAND THROUGH CANTOUCH
							changeAllInChainTestbench(firstNode);
							connectCanTouchTestbench(secondNode);
							connectNodesTestbench(p1, p2, child1, child2, WEAK);
							//DEBUG_PRINT("Node %d, with weak connection and Node %d, with strong connection (strong) connected with weak connection\n", firstNode->number, secondNode->number);
						}
					}
				}
				else {
					//both strong starting connection
					firstNode = p1;
					secondNode = p2;
					//check if the last node in the chain has weak connection
					DefectedNode* lastNode1 = findLastNodeInChainTestbench(firstNode);
					DefectedNode* lastNode2 = findLastNodeInChainTestbench(secondNode);
					if (lastNode1->touch[0].touchType == WEAK && lastNode2->touch[0].touchType == WEAK)
					{
						// STRONG (weak) -- STRONG (weak)
						//change all and add strong connection
						changeAllInChainTestbench(firstNode);
						changeAllInChainTestbench(secondNode);
						connectNodesTestbench(p1, p2, child1, child2, STRONG);
						//if one is a blossom then need to modify the internal connections
						if (p1->isBlossom)
							modifyInternalConnectionsTestbench(child1->directParent->myself, child1);
						if (p2->isBlossom)
							modifyInternalConnectionsTestbench(child2->directParent->myself, child2);
						deleteWeakConnectionsTestbench(firstNode);
						//DEBUG_PRINT("Node %d, with strong connection and Node %d, with strong connection connected with weak connection\n", firstNode->number, secondNode->number);
					}
					else {
						//create strong connection
						if (lastNode1->touch[0].touchType == STRONG && lastNode2->touch[0].touchType == STRONG)
						{
							// CHECK IF POSSIBLE
							//DEBUG_PRINT("ERRROR CANNOT HAVE 2 STABLE STRUCTURE TO EXPAND\n");
							//add to canTouch 
							p1->canTouch[p1->canTouchCount].node = p2;
							p2->canTouch[p2->canTouchCount].node = p1;
							p1->canTouch[p1->canTouchCount].mychild = child1;
							p2->canTouch[p2->canTouchCount].mychild = child2;
							p1->canTouch[p1->canTouchCount].otherchild = child2;
							p2->canTouch[p2->canTouchCount].otherchild = child1;
							p1->canTouchCount++;
							p2->canTouchCount++;

						}
						else {
							// STRONG (weak) -- STRONG (strong)
							if (lastNode1->touch[0].touchType == STRONG)
								connectCanTouchTestbench(firstNode);
							else
								connectCanTouchTestbench(secondNode);
							//create weak connection
							//NEED FROM STRONG PART TO CHECK IF CAN EXPAND THROUGH CANTOUCH
							connectNodesTestbench(p1, p2, child1, child2, WEAK);
							//DEBUG_PRINT("Node %d, with strong connection and Node %d, with strong connection connected with weak connection\n", firstNode->number, secondNode->number);
						}
					}
				}
			}
			else {
				DefectedNode* nodesInLoop[MAXELEMENTS];
				short nodesInLoopCount = 0;
				getDefectedForLoopTestbench(p2, p1, nodesInLoop, &nodesInLoopCount);
				//DEBUG_PRINT("Loop found between %d and %d with dimension %d\n", p1->number, p2->number, nodesInLoopCount);

				/*for ( i = 0; i < nodesInLoopCount; i++)
				{
					//DEBUG_PRINT("Node %d\n", nodesInLoop[i]->number);
				}*/
				//CREATE BLOSSOM
				//DEBUG_PRINT("NEED TO CREATE A BLOSSOM OF DIMENSION %d\n", nodesInLoopCount);
				if (nodesInLoopCount > 3)
				{
					printf("NOTDONE CREATION OF BLOSSOM DIM > 3\n");
				}
				createBlossomTestbench(nodesInLoop, nodesInLoopCount, p1, p2, child1, child2);
			}
		}

	}

}

//get the other node connected to the edge
Node* getOtherNodeTestbench(short k, Node* node, short d)
{
	Node* otherNode = NULL;
	//i sould not have to check the x and y
	if (k == NORTH && node->y > 0)
	{
		otherNode = &graph[node->y - 1][node->x];
	}
	else if (k == EAST && node->x < d)
	{
		otherNode = &graph[node->y][node->x + 1];
	}
	else if (k == SOUTH && node->y < d)
	{
		otherNode = &graph[node->y + 1][node->x];
	}
	else if (k == WEST && node->x > 0)
	{
		otherNode = &graph[node->y][node->x - 1];
	}
	return otherNode;
}



//If need to delete the canTouch only of the other node
void deleteOtherCanTouchTestbench(DefectedNode* node1, DefectedNode* node2)
{
	short i;
	if (node2->canTouchCount == 1)
	{
		node2->canTouchCount = 0;
		return;
	}
	// Find the canTouch in node1
	short canTouchIndex = -1;
	for ( i = 0; i < node2->canTouchCount; i++)
	{
		if (node2->canTouch[i].node == node1)
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
		if (canTouchIndex != node2->canTouchCount - 1)
		{
			for ( i = canTouchIndex; i < node2->canTouchCount - 1; i++)
			{
				node2->canTouch[i] = node2->canTouch[i + 1];
			}
		}
		node2->canTouchCount--;
	}
}

void processRetractDefectedNodesTestbench(short d, DefectedNode* defectedNode)
{

	short count = defectedNode->nodesCount;
	Node* nodes[MAXELEMENTS];
	short nCount = 0;
	short j,k;
	//else continue with the new ones
	for ( j = 0; j < count; j++)
	{
		Node* node = defectedNode->nodes[j];
		if (node != NULL)
		{
			//DEBUG_PRINT("Retract node %d\n", node->number);
			if (node->weightGot != 0)
			{
				//DEBUG_PRINT("Node %d is not retracted\n", node->number);
				for ( k = 0; k < 4; k++)
				{
					Edge* edge = node->edges[k];
					if (edge != NULL && edge->weight != MAXWEIGHT)
					{
						//DEBUG_PRINT("Node %d, Edge %d, Weight %d\n", node->number, edge->number, edge->weight);
						// Check if the node on the other side is has the same parent if yes don't retract
						Node* otherNode = getOtherNodeTestbench(k, node, d);
						//if the other node is connected to a different parent
						if (otherNode != NULL && otherNode->defectedparent != node->defectedparent)
						{
							//DEBUG_PRINT("Other node %d\n", otherNode->number);
							//retract the edge
							edge->weight += MAXWEIGHT / 2;
						}
						//
					}
				}
				node->weightGot -= MAXWEIGHT / 2;
				nodes[nCount] = node;
				nCount++;
				//DEBUG_PRINT("Node defected parent %d\n", node->defectedparent->number);
				//DEBUG_PRINT("Node blossom parent %d\n", node->blossomParent->number);
			}
			else {
				//DEBUG_PRINT("Node %d is already retracted\n", node->number);
				//if connected to node with same parent and not in nodes then add it
				//DEBUG_PRINT("%d ? %d && %d ? %d", node->defectedparent->x, node->x, node->defectedparent->y, node->y);
				if (node->defectedparent->x != node->x || node->defectedparent->y != node->y)
				{
					for ( k = 0; k < 4; k++)
					{
						Edge* edge = node->edges[k];
						//must be maxweight
						if (edge != NULL && edge->weight == 0)
						{
							//check if the other node is connected to the same parent
							Node* otherNode = getOtherNodeTestbench(k, node, d);
							if (otherNode != NULL && otherNode->defectedparent != NULL && otherNode->defectedparent == node->defectedparent)
							{
								short isIn = -1;
								//check if is the first time i see the node
								for (short l = 0; l < nCount; l++)
								{
									if (nodes[l] == otherNode)
									{
										isIn = 0;
										break;
									}
								}
								if (isIn == -1)
								{
									//DEBUG_PRINT("Adding node %d to be retracted next\n", otherNode->number);
									otherNode->weightGot -= MAXWEIGHT / 2;
									//DEBUG_PRINT("Node %d reduced weight\n", otherNode->number);
									for (short h = 0; h < 4; h++)
									{
										Edge* edge = otherNode->edges[h];
										if (edge != NULL && edge->weight == 0)
										{
											edge->weight += MAXWEIGHT / 2;
										}
									}
									nodes[nCount] = otherNode;
									nCount++;
								}
							}
						}
					}
					//DEBUG_PRINT("Node number %d \n", node->number);
					//DEBUG_PRINT("Defected Node number %d \n", graph[defectedNode->y][defectedNode->x].number);
					if (node != &graph[defectedNode->y][defectedNode->x])
					{
						node->blossomParent = NULL;
						node->defectedparent = NULL;
						//if there is an edge with weight 0 then the node must be set as some of the other nodes
						for ( k = 0; k < 4; k++)
						{
							Edge* edge = node->edges[k];
							if (edge != NULL && edge->weight == 0)
							{
								Node* otherNode = getOtherNodeTestbench(k, node, d);
								if (otherNode != NULL && otherNode->defectedparent != NULL)
								{
									//add the node to the expansion list
									otherNode->defectedparent->nodes[otherNode->defectedparent->nodesCount] = node;
									otherNode->defectedparent->nodesCount++;
									node->defectedparent = otherNode->defectedparent;
									//Assigned the highest parent to the node else the defected node
									if (otherNode->defectedparent->hasParent)
										node->blossomParent = otherNode->defectedparent->highestParent->myself;
									else
										node->blossomParent = otherNode->defectedparent;
									break;
								}
							}
						}

					}
					//else
						//DEBUG_PRINT("Node %d is a defected node, cannot delete parent\n", node->number);

				}
				else {
					//DEBUG_PRINT("Node %d is a defected node, cannot retract\n", node->number);
					nodes[nCount] = node;
					nCount++;
				}
			}
		}

	}
	for ( j = 0; j < nCount; j++)
	{
		defectedNode->nodes[j] = nodes[j];
	}
	defectedNode->nodesCount = nCount;
	//print all nodes to be retracted next
	//DEBUG_PRINT("Nodes to be retracted next:\n");
	/*
	for ( j = 0; j < defectedNode->nodesCount; j++)
	{
		if (defectedNode->nodes[j] != NULL)
		{
			//DEBUG_PRINT(" % d\n", defectedNode->nodes[j]->number);
		}
	}*/
	//Delete all canTouch connections
	//DEBUG_PRINT("Deleting all canTouch connections of node %d\n", defectedNode->number);
	for ( j = 0; j < defectedNode->canTouchCount; j++)
	{
		DefectedNode* otherNode = defectedNode->canTouch[j].node;
		//DEBUG_PRINT("Deleting connection between %d and %d\n", defectedNode->number, otherNode->number);
		////DEBUG_PRINT all canTouch connections of other node

		deleteOtherCanTouchTestbench(defectedNode, otherNode);
		//DEBUG_PRINT("Remaining canTouch connections of node %d\n", otherNode->number);

	}
	defectedNode->canTouchCount = 0;
	defectedNode->lastRoundModified = actualRound;
}

//TODO find connections thorugh the defected nodes
void processExpandDefectedNodesTestbench(short d, DefectedNode* defectedNode)
{
	bool expanded = false;
	short count = defectedNode->nodesCount;
	short j;
	//else continue with the new ones
	for ( j = 0; j < count; j++)
	{
		Node* node = defectedNode->nodes[j];
		short edgeSum = 0;
		if (node != NULL && !node->isVirtual)
		{
			//DEBUG_PRINT("Expand node %d\n", node->number);
			for (short k = 0; k < 4; k++)
			{
				Edge* edge = node->edges[k];
				if (edge != NULL && edge->weight != 0)
				{
					////DEBUG_PRINT("Node %d, Edge %d, Weight %d\n", node->number, edge->number, edge->weight);
					edge->weight -= MAXWEIGHT / 2;
					edgeSum += edge->weight;
					if (edge->weight == 0)
					{
						Node* otherNode = getOtherNodeTestbench(k, node, d);
						if (otherNode != NULL && otherNode->defectedparent != NULL && (node->weightGot == 0 || otherNode->defectedparent->x != otherNode->x || otherNode->defectedparent->y != otherNode->y))
						{
							////DEBUG_PRINT("Other node %d\n", otherNode->number);
							//if last touches was not between the same nodes\
									//then add the touches
							////DEBUG_PRINT("Defected node %d connected to defected node %d\n", node->blossomParent->number, otherNode->blossomParent->number);
							//DEBUG_PRINT("Inner node %d connected to inner node %d\n", node->defectedparent->number, otherNode->defectedparent->number);
							//IF I DO THIS I NEED TO CHANGE EVERY TIME I CREATE A BLOSSOM
							//ELSE I COULD USE THE HIGHET PARENT OF THE DEFECTED PARENT DIRECTLY
							//touchesProcessingTestbench(node->defectedparent->highestParent, otherNode->defectedparent->highestParent, node->defectedparent, otherNode->defectedparent);

							//NEED TO CHECK IF OTHERNODE IS THE DEFECTED NODE STARTING POINT NEED TO SEE IN THE OPPOSITE DIRENCTION IF THERE IS A NODE THAT IS ALREADY CONNECTED
							// -> THE CONNECTION WILL BE BETWEEN THE 2 DEFECTED NODE OPPISITE OF THE CENTRAL DEFECTED NODE
							// OR I CAN CHECK IN ALL THE DIRECTION TO SEE IF THERE IS ONE THAT IS ALREADY CONNECTED
							// CAN CHECK IF OTHERNODE->TOUCHCOUNT == 2->NEED TO FORM A BLOSSOM WITH THE OPPOSITE NODE
							if (node->defectedparent->hasParent && otherNode->defectedparent->hasParent)
								touchesProcessingTestbench(node->defectedparent->highestParent->myself, otherNode->defectedparent->highestParent->myself, node->defectedparent, otherNode->defectedparent);
							else if (node->defectedparent->hasParent)
								touchesProcessingTestbench(node->defectedparent->highestParent->myself, otherNode->defectedparent, node->defectedparent, otherNode->defectedparent);
							else if (otherNode->defectedparent->hasParent)
								touchesProcessingTestbench(node->defectedparent, otherNode->defectedparent->highestParent->myself, node->defectedparent, otherNode->defectedparent);
							else
								touchesProcessingTestbench(node->defectedparent, otherNode->defectedparent, node->defectedparent, otherNode->defectedparent);
						}
						else if (otherNode != NULL && otherNode->defectedparent != NULL)
						{
							//DEBUG_PRINT("The node in contact is a defected node serching for near defected nodes\n");
							for (short i = 0; i < 4; i++) {
								if (i != (3 - k)) {
									Edge* otherEdge = otherNode->edges[i];

									if (otherEdge != NULL && otherEdge->weight == 0) {
										Node* otherOtherNode = getOtherNodeTestbench(i, otherNode, d);
										if (otherOtherNode != NULL && otherOtherNode->defectedparent != NULL)
										{
											////DEBUG_PRINT("OtherOther node %d\n", otherOtherNode->number);
											//if last touches was not between the same nodes\
													//then add the touches
											////DEBUG_PRINT("Defected node %d connected to defected node %d\n", node->blossomParent->number, otherNode->blossomParent->number);
											//DEBUG_PRINT("Inner node %d connected to inner node %d\n", node->defectedparent->number, otherOtherNode->defectedparent->number);
											//IF I DO THIS I NEED TO CHANGE EVERY TIME I CREATE A BLOSSOM
											//ELSE I COULD USE THE HIGHET PARENT OF THE DEFECTED PARENT DIRECTLY
											//touchesProcessingTestbench(node->defectedparent->highestParent, otherNode->defectedparent->highestParent, node->defectedparent, otherNode->defectedparent);

											//NEED TO CHECK IF OTHERNODE IS THE DEFECTED NODE STARTING POINT NEED TO SEE IN THE OPPOSITE DIRENCTION IF THERE IS A NODE THAT IS ALREADY CONNECTED
											// -> THE CONNECTION WILL BE BETWEEN THE 2 DEFECTED NODE OPPISITE OF THE CENTRAL DEFECTED NODE
											// OR I CAN CHECK IN ALL THE DIRECTION TO SEE IF THERE IS ONE THAT IS ALREADY CONNECTED
											// CAN CHECK IF OTHERNODE->TOUCHCOUNT == 2->NEED TO FORM A BLOSSOM WITH THE OPPOSITE NODE
											if (node->defectedparent->hasParent && otherOtherNode->defectedparent->hasParent)
												touchesProcessingTestbench(node->defectedparent->highestParent->myself, otherOtherNode->defectedparent->highestParent->myself, node->defectedparent, otherOtherNode->defectedparent);
											else if (node->defectedparent->hasParent)
												touchesProcessingTestbench(node->defectedparent->highestParent->myself, otherOtherNode->defectedparent, node->defectedparent, otherOtherNode->defectedparent);
											else if (otherOtherNode->defectedparent->hasParent)
												touchesProcessingTestbench(node->defectedparent, otherOtherNode->defectedparent->highestParent->myself, node->defectedparent, otherOtherNode->defectedparent);
											else
												touchesProcessingTestbench(node->defectedparent, otherOtherNode->defectedparent, node->defectedparent, otherOtherNode->defectedparent);
										}
									}
								}
							}
						}
						else if (otherNode != NULL)
						{
							//DEBUG_PRINT("Defected node %d got %d connected\n", defectedNode->number, otherNode->number);
							otherNode->defectedparent = node->defectedparent;
							//TODO CHECK IF CORRECT PARENT
							//Assigned the highest parent to the node else the defected node
							if (node->defectedparent->hasParent)
								otherNode->blossomParent = node->defectedparent->highestParent->myself;
							else
								otherNode->blossomParent = node->defectedparent;
							//defectedNode->nodes[j] = NULL; done if all edges done
							if (!otherNode->isVirtual)
							{
								defectedNode->nodes[defectedNode->nodesCount] = otherNode;
								defectedNode->nodesCount++;
							}
							else {
								defectedNode->nodes[defectedNode->nodesCount] = otherNode;
								defectedNode->nodesCount++;
								defectedNode->connectedToVirtual = true;
								//also highest parent connected to virtual
								if (defectedNode->hasParent)
								{
									defectedNode->highestParent->myself->connectedToVirtual = true;
									//DEBUG_PRINT("Highest parent %d connected to virtual\n", defectedNode->highestParent->myself->number);
								}
								//DEBUG_PRINT("Defected node %d connected to virtual\n", defectedNode->number);
							}
							expanded = true;
						}
					}
				}
			}
			if (edgeSum == 0 && expanded)
			{
				defectedNode->nodes[j] = NULL;
			}
			node->weightGot += MAXWEIGHT / 2;
		}
	}
	//print all nodes to be expanded next
	//DEBUG_PRINT("Nodes to be expanded next:\n");
	/*
	for ( j = 0; j < defectedNode->nodesCount; j++)
	{
		if (defectedNode->nodes[j] != NULL)
		{
			//DEBUG_PRINT(" %d\n", defectedNode->nodes[j]->number);
		}
	}*/
	//if i created a blossom this actualRound i need to update all remaining nodes to the highest parent
	if (defectedNode->hasParent && defectedNode->highestParent->myself->lastRoundModified == actualRound)
	{
		//all nodes blossom parent to highest parent
		for ( j = 0; j < defectedNode->nodesCount; j++)
		{
			Node* node = defectedNode->nodes[j];
			if (node != NULL)
			{

				node->blossomParent = defectedNode->highestParent->myself;
				//DEBUG_PRINT("Node %d blossom parent %d and parent %d\n", node->number, node->blossomParent->number, node->defectedparent->number);
			}
		}
	}
	defectedNode->lastRoundModified = actualRound;
}

void deleteBlossomTestbench(DefectedNode* defectedNode)
{
	Blossom* blossom = defectedNode->myself;
	BlossomQueue queue;
	DefectedNode* defected[(MAXELEMENTS * (MAXELEMENTS - 1))] = { NULL };
	short defectedCount = 0;

	DefectedNode* firstDefectedNode = NULL;
	short firstTouch = -1;
	DefectedNode* secondDefectedNode = NULL;
	short secondTouch = -1;
	short dimension = blossom->childrenBlossomCount + blossom->childrenDefectedNodesCount;
	short i,j,k;
	//TODO
	//update the parent of the defected nodes
	for ( i = 0; i < blossom->childrenDefectedNodesCount; i++)
	{
		blossom->childrenDefectedNodes[i]->hasParent = false;
		blossom->childrenDefectedNodes[i]->highestParent = NULL;
		blossom->childrenDefectedNodes[i]->directParent = NULL;
		defected[defectedCount] = blossom->childrenDefectedNodes[i];
		defectedCount++;
		for ( j = 0; j < blossom->myself->touchCount; j++)
		{
			if (blossom->myself->touch[j].mychild == blossom->childrenDefectedNodes[i])
			{
				if (firstDefectedNode == NULL)
				{
					firstDefectedNode = blossom->childrenDefectedNodes[i];
					firstTouch = j;
				}
				else {
					secondDefectedNode = blossom->childrenDefectedNodes[i];
					secondTouch = j;
				}
			}
		}
		//for each node for expansion of the defected node the blossomParent is the defected node
		for ( k = 0; k < blossom->childrenDefectedNodes[i]->nodesCount; k++)
		{
			if (blossom->childrenDefectedNodes[i]->nodes[k] != NULL)
				blossom->childrenDefectedNodes[i]->nodes[k]->blossomParent = blossom->childrenDefectedNodes[i];
		}
	}
	//put each blossom
	for (short l = 0; l < blossom->childrenBlossomCount; l++)
	{
		blossom->childrenBlossoms[l]->parent = NULL;
		blossom->childrenBlossoms[l]->myself->hasParent = false;
		blossom->childrenBlossoms[l]->myself->highestParent = NULL;
		blossom->childrenBlossoms[l]->myself->directParent = NULL;
		initializeBlossomQueueTestbench(&queue);
		enqueueBlossomTestbench(&queue, blossom->childrenBlossoms[l]);
		defected[defectedCount] = blossom->childrenBlossoms[l]->myself;
		defectedCount++;
		while (!isBlossomQueueEmptyTestbench(&queue))
		{
			Blossom* currentBlossom = dequeueBlossomTestbench(&queue);
			//put each blossom children in the queue
			for ( i = 0; i < currentBlossom->childrenBlossomCount; i++)
			{
				enqueueBlossomTestbench(&queue, currentBlossom->childrenBlossoms[i]);
			}
			//put each defectedNode children in defectedNodes
			for ( i = 0; i < currentBlossom->childrenDefectedNodesCount; i++)
			{
				//update highest parent of each defected node as the first blossom children of the deleted blossom
				currentBlossom->childrenDefectedNodes[i]->highestParent = blossom->childrenBlossoms[l];
				for ( j = 0; j < currentBlossom->childrenDefectedNodes[i]->touchCount; j++)
				{
					if (blossom->myself->touch[j].mychild == currentBlossom->childrenDefectedNodes[i])
					{
						if (firstDefectedNode == NULL)
						{
							firstDefectedNode = blossom->childrenBlossoms[l]->myself;
							firstTouch = j;
						}
						else {
							secondDefectedNode = blossom->childrenBlossoms[l]->myself;
							secondTouch = j;
						}
					}
				}
				//TODO IF NODE BLOSSOMPARENT IS THE BLOSSOM THEN CHANGE IT TO THE HIGHEST PARENT
				for ( k = 0; k < currentBlossom->childrenDefectedNodes[i]->nodesCount; k++)
				{
					if (currentBlossom->childrenDefectedNodes[i]->nodes[k] != NULL)
						currentBlossom->childrenDefectedNodes[i]->nodes[k]->blossomParent = blossom->childrenBlossoms[l]->myself;
				}
			}
		}
	}
	if (firstDefectedNode == NULL || secondDefectedNode == NULL)
	{
		DEBUG_PRINT("ERROR: Could not find the defected nodes\n");
	}
	else if (firstDefectedNode == secondDefectedNode && dimension == 3) {
		//cut both touch of firstDefectedNode
		DefectedNode* defectedTouch0 = firstDefectedNode->touch[0].node;
		DefectedNode* lastNode = defectedTouch0;
		DefectedNode* currentNode = NULL;
		deleteTouchTestbench(firstDefectedNode, firstDefectedNode->touch[1].node, false);
		deleteTouchTestbench(firstDefectedNode, firstDefectedNode->touch[0].node, false);
		defectedTouch0->expandOrReduce = 0;
		//put all the nodes in the chain to expand,reduce =0
		currentNode = defectedTouch0->touch[0].node;

		while (currentNode != NULL)
		{
			//DEBUG_PRINT("Current node %d\n", currentNode->number);
			currentNode->expandOrReduce = 0;
			if (currentNode->touch[0].node == lastNode) {
				lastNode = currentNode;
				if (currentNode->touchCount == 2)
					//if cycle check also that is not equal to the first node
					currentNode = currentNode->touch[1].node;
				else {
					//DEBUG_PRINT("Last node %d\n", currentNode->number);
					currentNode = NULL;
				}
			}
			else
				currentNode = currentNode->touch[0].node;
		}
		if (defectedTouch0->touch[0].touchType != STRONG)
		{
			changeAllInChainTestbench(defectedTouch0);
			deleteWeakConnectionsTestbench(defectedTouch0);
		}
		//connect firstNode with the nodes connected to the father

	}
	else if (dimension == 3) {
		//cut the edge between firstDefectedNode and secondDefectedNode
		//TODO
		//DEBUG_PRINT("Cutting the edge between %d and %d\n", firstDefectedNode->number, secondDefectedNode->number);
		deleteTouchTestbench(firstDefectedNode, secondDefectedNode, false);
	}
	else {
		//TODO
		DEBUG_PRINT("NOT DONE: Dimension > 3\n");
	}
	//for each node connected to the blossom delete the connection and
	if (firstDefectedNode != NULL && secondDefectedNode != NULL && firstTouch != -1 && secondTouch != -1)
	{

		DefectedNode* defectedTouch = blossom->myself->touch[firstTouch].node;
		DefectedNode* defectedTouchOtherChild = blossom->myself->touch[firstTouch].otherchild;
		DefectedNode* defectedTouchMyChild = blossom->myself->touch[firstTouch].mychild;
		bool touchType = blossom->myself->touch[firstTouch].touchType;
		//delete the touch
		deleteTouchTestbench(blossom->myself, defectedTouch, true);
		//DEBUG_PRINT("Connecting %d with %d with touchType %d\n", defectedTouch->number, firstDefectedNode->number, touchType);
		connectNodesTestbench(firstDefectedNode, defectedTouch, defectedTouchMyChild, defectedTouchOtherChild, touchType);
		defectedTouch = blossom->myself->touch[0].node;
		defectedTouchOtherChild = blossom->myself->touch[0].otherchild;
		defectedTouchMyChild = blossom->myself->touch[0].mychild;
		touchType = blossom->myself->touch[0].touchType;
		//delete the touch
		deleteTouchTestbench(blossom->myself, defectedTouch, true);
		//DEBUG_PRINT("Connecting %d with %d with touchType %d\n", defectedTouch->number, secondDefectedNode->number, touchType);
		connectNodesTestbench(secondDefectedNode, defectedTouch, defectedTouchMyChild, defectedTouchOtherChild, touchType);
		defectedNode->usable = false;
	}
	else {
		DEBUG_PRINT("ERROR: During Blossom Deletion\n");
	}

	//	TODO DECISION ON WHICH EDGE TO CUT
	//	FOR BOTH CONNECTIONS OF THE BLOSSOM
	//	USING ALL THE BLOSSOM EXTERNAL CONNECTIONS UNITE THE OTHER NODE WHIT THE HIGHEST PARENT LEFT (THE FIRST CHILDREN OF THE BLOSSOM) 
	//	-> NEED TO MODIFY THE OTHER CONNECTIONS SO IT KNOWS IT IS ALREADY CONNECTED TO HIM
	//	-> NEED TO MODIFY THE BLOSSOM CHILDREN SO THEY KNOW THEY ARE CONNECTED TO THE OTHER NODE 
}

void printAllElementsTestbench()
{/*
	//DEBUG_PRIORITY_PRINT("\n");
	//DEBUG_PRIORITY_PRINT("actualRound %d\n", actualRound);
	//DEBUG_PRIORITY_PRINT("All elements\n");
	for (short i = 0; i < defectedNodesCount; i++)
	{
		DefectedNode* defectedNode = &defectedNodes[i];
		if (defectedNode->usable)
		{
			//DEBUG_PRIORITY_PRINT("Defected node %d\n", defectedNode->number);
			if (defectedNode->hasParent) {
				//DEBUG_PRIORITY_PRINT("The Highest parent of the defected node %d is %d\n", defectedNode->number, defectedNode->highestParent->myself->number);
				//DEBUG_PRIORITY_PRINT("The direct parent of the defected node %d is %d\n", defectedNode->number, defectedNode->directParent->myself->number);
			}
			//DEBUG_PRIORITY_PRINT("Defected node %d has %d touches\n", defectedNode->number, defectedNode->touchCount);
			for (short j = 0; j < defectedNode->touchCount; j++)
			{
				//DEBUG_PRIORITY_PRINT("Defected node %d can touch %d with touchtype ", defectedNode->number, defectedNode->touch[j].node->number);
				if (defectedNode->touch[j].touchType) {
					//DEBUG_PRIORITY_PRINT("STRONG\n");
				}
				else {
					//DEBUG_PRIORITY_PRINT("WEAK\n");
				}
				if (defectedNode->touch[j].node->isBlossom)
					//DEBUG_PRIORITY_PRINT("Connected to otherchild %d\n", defectedNode->touch[j].otherchild->number);
				if (defectedNode->isBlossom)
					//DEBUG_PRIORITY_PRINT("Connected to mychild %d\n", defectedNode->touch[j].mychild->number);
			}
			if (defectedNode->connectedToVirtual)
				//DEBUG_PRIORITY_PRINT("Defected node %d is connected to virtual\n", defectedNode->number);
			if (defectedNode->expandOrReduce == 1)
				//DEBUG_PRIORITY_PRINT("Defected node %d is to be expanded\n", defectedNode->number);
			else if (defectedNode->expandOrReduce == -1)
				//DEBUG_PRIORITY_PRINT("Defected node %d is to be reduced\n", defectedNode->number);
			else if (defectedNode->expandOrReduce == 0)
				//DEBUG_PRIORITY_PRINT("Defected node %d is stable\n", defectedNode->number);
		}
		else {
			//DEBUG_PRIORITY_PRINT("Defected node %d is not usable\n", defectedNode->number);
		}
	}
	//DEBUG_PRIORITY_PRINT("\n");
	 *
	 */
}
//Write a function equals to roundcheck but that uses the queue
//TODO delete blossom
bool checkGoodVirtualTestbench(DefectedNode* connectedToVirtual) {
	////DEBUG_PRIORITY_PRINT("Check good virtual\n");
	short i,j;
	if (connectedToVirtual->canTouchCount == 1 || connectedToVirtual->touchCount == 0)
		return true;
	else {
		DefectedNode* firstNode = NULL;
		DefectedNode* lastNode = connectedToVirtual;
		int count = 1;
		Queue queue;
		for ( j = 0; j < connectedToVirtual->touchCount; j++)
		{
			firstNode = connectedToVirtual->touch[j].node;
			initializeQueueTestbench(&queue);
			enqueueTestbench(&queue, firstNode);
			while (!isQueueEmptyTestbench(&queue))
			{
				DefectedNode* currentNode = dequeueTestbench(&queue);
				for ( i = 0; i < currentNode->touchCount; i++)
				{
					DefectedNode* nextNode = currentNode->touch[i].node;
					if (nextNode != connectedToVirtual && nextNode != lastNode)
					{
						enqueueTestbench(&queue, nextNode);
						count++;
					}

				}
				lastNode = currentNode;
			}
			if (count % 2 == 1)
			{
				//DEBUG_PRIORITY_PRINT("Node %d has an odd number of nodes in the chain\n", firstNode->number);
				return false;
			}
		}

	}
	return true;
}

bool roundcheckWithQueueTestbench()
{
	//DEBUG_PRINT("actualRound check with queue\n");
	short i;
	short usedNodes[MAXDEFECTEDNODES];
	Queue queue;
	bool needExpansion = false;
	bool hasVirtual = false;

	for ( i = 0; i < defectedNodesCount; i++)
	{
		if (!defectedNodes[i].usable || defectedNodes[i].hasParent)
			usedNodes[i] = -2;
		else
			usedNodes[i] = -1;
	}
	for ( i = 0; i < defectedNodesCount; i++)
	{
		// eliminate array of pointers
		DefectedNode* defectedNode = &defectedNodes[i];
		short count = 1;
		//Check if the node is not used
		// if the defected node is not used and has no parent then add it to the stack
		// Only top level nodes are considered in the execution
		/*
		if (defectedNode->usable) {
			//DEBUG_PRINT("Check defectedNode %d\n", defectedNode->number);
			//DEBUG_PRINT("Defected node %d has %d touches\n", defectedNode->number, defectedNode->touchCount);
			//DEBUG_PRINT("with usedNodes %d\n", usedNodes[i]);
		}
		*/
		if (defectedNode->usable && usedNodes[i] == -1 && defectedNode->touchCount < 2)
		{
			//DEBUG_PRINT("Checking node %d\n", defectedNode->number);
			//start from top level node in the chain
			hasVirtual = false;
			initializeQueueTestbench(&queue);
			enqueueTestbench(&queue, defectedNode);
			DefectedNode* lastNode = defectedNode;
			usedNodes[defectedNode->number] = 0;
			while (!isQueueEmptyTestbench(&queue))
			{
				DefectedNode* currentNode = dequeueTestbench(&queue);
				if (currentNode->connectedToVirtual && !hasVirtual)
					hasVirtual = checkGoodVirtualTestbench(currentNode);
				//setup for non expansion
				currentNode->expandOrReduce = 0;
				for ( short j = 0; j < currentNode->touchCount; j++)
				{
					DefectedNode* nextNode = currentNode->touch[j].node;
					// Ignore the return touch of the next node that is the same as the previous
					if (nextNode != lastNode)
					{
						count++;
						usedNodes[currentNode->touch[j].node->number] = usedNodes[currentNode->number] + 1;
						//print all what was done
						//DEBUG_PRINT("Added node %d to the queue\n", nextNode->number);
						enqueueTestbench(&queue, nextNode);
					}
				}
				lastNode = currentNode;
			}
			if (count % 2 == 1 && !hasVirtual)
			{
				//DEBUG_PRINT("Node %d has an odd number of nodes in the chain\n", defectedNode->number);
				needExpansion = true;
				initializeQueueTestbench(&queue);
				enqueueTestbench(&queue, defectedNode);
				defectedNode->expandOrReduce = 1;
				lastNode = defectedNode;
				while (!isQueueEmptyTestbench(&queue))
				{
					DefectedNode* currentNode = dequeueTestbench(&queue);
					for ( short h = 0; h < currentNode->touchCount; h++)
					{
						DefectedNode* nextNode = currentNode->touch[h].node;
						// Ignore the return touch of the next node that is the same as the previous
						if (nextNode != lastNode)
						{
							if (currentNode->expandOrReduce == -1)
							{
								nextNode->expandOrReduce = 1;
								//print all what was done
								//DEBUG_PRINT("Added node %d to the queue\n", nextNode->number);
								enqueueTestbench(&queue, nextNode);
							}
							else {
								if (nextNode->isBlossom && nextNode->myself->dimension == 0) {
									//DEBUG_PRIORITY_PRINT("Node %d is a blossom with dimension 0\n", nextNode->number);
									deleteBlossomTestbench(nextNode);
									h = -1;
								}
								else {
									nextNode->expandOrReduce = -1;
									//print all what was done
									//DEBUG_PRINT("Added node %d to the queue\n", nextNode->number);
									enqueueTestbench(&queue, nextNode);
								}

							}

						}
					}
					lastNode = currentNode;
				}

			}
			//else
				//DEBUG_PRINT("Node %d has a stable chain\n", defectedNode->number);

		}
	}
	printAllElementsTestbench();
	return needExpansion;
}

void checkDefNodesTestbench()
{
	DefectedNode* defectedNode0 = &defectedNodes[0];
	DefectedNode* defectedNode1 = &defectedNodes[1];
	DefectedNode* defectedNode2 = &defectedNodes[2];
	DefectedNode* defectedNode3 = &defectedNodes[3];
	DefectedNode* defectedNode4 = &defectedNodes[4];
	DefectedNode* defectedNode5 = &defectedNodes[5];
	DefectedNode* defectedNode6 = &defectedNodes[6];

	connectNodesTestbench(defectedNode0, defectedNode1, defectedNode0, defectedNode1, STRONG);
	connectNodesTestbench(defectedNode1, defectedNode2, defectedNode1, defectedNode2, WEAK);
	connectNodesTestbench(defectedNode2, defectedNode3, defectedNode2, defectedNode3, STRONG);
	connectNodesTestbench(defectedNode3, defectedNode4, defectedNode3, defectedNode4, WEAK);
	connectNodesTestbench(defectedNode5, defectedNode6, defectedNode5, defectedNode6, STRONG);
	touchesProcessingTestbench(defectedNode0, defectedNode2, defectedNode0, defectedNode2);
	touchesProcessingTestbench(defectedNode1->highestParent->myself, defectedNode4, defectedNode1, defectedNode4);
	touchesProcessingTestbench(defectedNode4->highestParent->myself, defectedNode5, defectedNode4, defectedNode5);

	short i,j;
	//DEBUG_PRINT("CHECK 1\n");
	//print all connections of all defected nodes
	for ( i = 0; i < defectedNodesCount; i++)
	{
		DefectedNode* defectedNode = &defectedNodes[i];
		//DEBUG_PRINT("Defected node %d\n", defectedNode->number);
		//DEBUG_PRINT("Defected node %d has touch count of %d\n", defectedNode->number, defectedNode->touchCount);
		for ( j = 0; j < defectedNode->touchCount; j++)
		{
			//DEBUG_PRINT("Defected node %d touches %d with %d\n", defectedNode->number, defectedNode->touch[j].node->number, defectedNode->touch[j].touchType);
		}
	}
	////DEBUG_PRINT for all blossom all children
	for ( i = 0; i < blossomsCount; i++)
	{
		Blossom* blossom = &blossoms[i];
		if (blossom != NULL)
		{
			//DEBUG_PRINT("Blossom %d that is defectedNode %d\n", i, blossom->myself->number);
			//DEBUG_PRINT("Blossom %d has %d children\n", i, blossom->childrenDefectedNodesCount);
			//create a queue with all blossom children
			BlossomQueue queue;
			initializeBlossomQueueTestbench(&queue);
			for ( j = 0; j < blossom->childrenBlossomCount; j++)
			{
				//DEBUG_PRINT("Defected Node %d has blossom child defected node  %d\n", blossom->myself->number, blossom->childrenBlossoms[j]->myself->number);
				enqueueBlossomTestbench(&queue, blossom->childrenBlossoms[j]);
			}
			for ( j = 0; j < blossom->childrenDefectedNodesCount; j++)
			{
				//DEBUG_PRINT("Blossom %d has child %d\n", i, blossom->childrenDefectedNodes[j]->number);
			}
			while (!isBlossomQueueEmptyTestbench(&queue))
			{
				Blossom* currentBlossom = dequeueBlossomTestbench(&queue);
				//put each blossom children in the queue
				for ( j = 0; j < currentBlossom->childrenBlossomCount; j++)
				{
					//DEBUG_PRINT("Defected node %d has blossom child defected node%d\n", currentBlossom->myself->number, blossom->childrenBlossoms[j]->myself->number);
					enqueueBlossomTestbench(&queue, currentBlossom->childrenBlossoms[j]);
				}
				//put each defectedNode children in defectedNodes
				/*
				for ( j = 0; j < currentBlossom->childrenDefectedNodesCount; j++)
				{
					//DEBUG_PRINT("Blossom Defectednode %d has child %d\n", currentBlossom->myself->number, currentBlossom->childrenDefectedNodes[j]->number);
				}
				*/
			}
		}
	}
	/*
	connectNodesTestbench(defectedNode3, defectedNode4, defectedNode3, defectedNode4, STRONG);
	deleteWeakConnectionsTestbench(defectedNode1);
	//DEBUG_PRINT("CHECK 1\n");
	DefectedNode* lastNode = findLastNodeInChainTestbench(defectedNode1);
	//DEBUG_PRINT("Last node in chain is %d, and the touch is %d and is touching %d\n", lastNode->number,lastNode->touch[0].touchType,lastNode->touch[0].node->number);
	printAllInChainTestbench(defectedNode1);
	//DEBUG_PRINT("CHECK 2\n");
	lastNode = findLastNodeInChainTestbench(defectedNode3);
	//DEBUG_PRINT("Last node in chain is %d, and the touch is %d and is touching %d\n", lastNode->number, lastNode->touch[0].touchType, lastNode->touch[0].node->number);
	printAllInChainTestbench(defectedNode4);
	*/
}

void findAllBlossomStrongConnectionTestbench(DefectedNode* parent, PairDefectedNode* pairs, short* pairCount, short d) {
	BlossomQueue queue;
	short j,k,l;
	initializeBlossomQueueTestbench(&queue);
	enqueueBlossomTestbench(&queue, parent->myself);
	//DEBUG_PRINT("Adding blossom %d to the queue\n", parent->number);
	while (!isBlossomQueueEmptyTestbench(&queue))
	{
		Blossom* currentBlossom = dequeueBlossomTestbench(&queue);
		//put each blossom children in the queue
		for ( j = 0; j < currentBlossom->childrenBlossomCount; j++)
		{
			//DEBUG_PRINT("Adding blossom %d to the queue\n", currentBlossom->childrenBlossoms[j]->myself->number);
			enqueueBlossomTestbench(&queue, currentBlossom->childrenBlossoms[j]);
			//if a blossom has a strong connection with another blossom or defected node that are children of the same parent then add it to the pairs
			for ( k = 0; k < currentBlossom->childrenBlossoms[j]->myself->touchCount; k++) {
				DefectedNode* otherNode = NULL;
				//
				if (currentBlossom->childrenBlossoms[j]->myself->touch[k].touchType == STRONG)
				{
					//DEBUG_PRINT("Blossom %d has a strong connection with %d\n", currentBlossom->childrenBlossoms[j]->myself->number, currentBlossom->childrenBlossoms[j]->myself->touch[k].node->number);
					for ( l = 0; l < currentBlossom->childrenBlossomCount; l++)
					{
						if (currentBlossom->childrenBlossoms[l]->myself == currentBlossom->childrenBlossoms[j]->myself->touch[k].node && l > k)
						{
							otherNode = currentBlossom->childrenBlossoms[l]->myself;
							break;
						}
					}
					if (otherNode == NULL)
					{
						for ( l = 0; l < currentBlossom->childrenDefectedNodesCount; l++)
						{
							if (currentBlossom->childrenDefectedNodes[l] == currentBlossom->childrenBlossoms[j]->myself->touch[k].node)
							{
								otherNode = currentBlossom->childrenDefectedNodes[l];
								break;
							}
						}
					}
					if (otherNode != NULL)
					{
						pairs[*pairCount].node1 = currentBlossom->childrenBlossoms[j]->myself->touch[k].mychild->y * (d + 1) + currentBlossom->childrenBlossoms[j]->myself->touch[k].mychild->x;
						pairs[*pairCount].node2 = currentBlossom->childrenBlossoms[j]->myself->touch[k].otherchild->y * (d + 1) + currentBlossom->childrenBlossoms[j]->myself->touch[k].otherchild->x;
						//DEBUG_PRINT("Adding pair %d %d\n", pairs[*pairCount].node1, pairs[*pairCount].node2);
						(*pairCount)++;
					}
				}
			}
		}
		//DEBUG_PRINT("Going to defected nodes\n");
		//put each defectedNode children in defectedNodes
		for ( j = 0; j < currentBlossom->childrenDefectedNodesCount; j++)
		{
			DefectedNode* defectedNode = currentBlossom->childrenDefectedNodes[j];
			for ( k = 0; k < defectedNode->touchCount; k++)
			{
				if (defectedNode->touch[k].touchType == STRONG && !defectedNode->touch[k].node->isBlossom)
				{
					for ( l = 0; l < currentBlossom->childrenDefectedNodesCount; l++)
					{
						if (currentBlossom->childrenDefectedNodes[l] == defectedNode->touch[k].node && l > j)
						{
							pairs[*pairCount].node1 = defectedNode->touch[k].mychild->y * (d + 1) + defectedNode->touch[k].mychild->x;
							pairs[*pairCount].node2 = defectedNode->touch[k].otherchild->y * (d + 1) + defectedNode->touch[k].otherchild->x;
							//DEBUG_PRINT("Adding pair %d %d\n", pairs[*pairCount].node1, pairs[*pairCount].node2);
							(*pairCount)++;
						}
					}
				}
			}
		}
	}
}

void constructionOfPairsTestbench(DefectedNode* defectedStrongNode, Queue* queue, PairDefectedNode* pairs, short* pairsCount, short d) {
	initializeQueueTestbench(queue);
	enqueueTestbench(queue, defectedStrongNode);
	DefectedNode* lastNode = defectedStrongNode;
	while (!isQueueEmptyTestbench(queue))
	{
		DefectedNode* currentNode = dequeueTestbench(queue);
		for (short i = 0; i < currentNode->touchCount; i++)
		{
			DefectedNode* nextNode = currentNode->touch[i].node;
			// Ignore the return touch of the next node that is the same as the previous
			if (nextNode != lastNode)
			{
				//if the connection is STRONG ADD TO PAIRS
				if (currentNode->touch[i].touchType == STRONG)
				{
					//DEBUG_PRINT("Adding pair %d %d\n", currentNode->touch[i].mychild, currentNode->touch[i].otherchild);
					pairs[*pairsCount].node1 = currentNode->touch[i].mychild->y * (d + 1) + currentNode->touch[i].mychild->x;
					pairs[*pairsCount].node2 = currentNode->touch[i].otherchild->y * (d + 1) + currentNode->touch[i].otherchild->x;
					(*pairsCount)++;
					if (currentNode->isBlossom)
						findAllBlossomStrongConnectionTestbench(currentNode, pairs, pairsCount, d);

					if (nextNode->isBlossom)
						findAllBlossomStrongConnectionTestbench(nextNode, pairs, pairsCount, d);

				}
				enqueueTestbench(queue, nextNode);
			}
		}
		lastNode = currentNode;
	}
}

void findVirtualInnerNodeTestbench(DefectedNode* toVirtual, PairDefectedNode* pairs, short* pairsCount, short d) {
	//Remember to do a strong connection between the virtual node and the inner node
	Blossom* blossom = toVirtual->myself;
	bool done = false;
	short i;
	short position = -1;
	//check if direct defected node is connected to the virtual node
	for ( i = 0; i < blossom->childrenDefectedNodesCount && !done; i++)
	{
		if (blossom->childrenDefectedNodes[i]->connectedToVirtual)
		{
			//printf("Defected node %d is connected to virtual\n", blossom->childrenDefectedNodes[i]->number);
			position = blossom->childrenDefectedNodes[i]->number;
			modifyInternalConnectionsTestbench(blossom->childrenDefectedNodes[i]->directParent->myself, blossom->childrenDefectedNodes[i]);
			done = true;
		}
	}
	BlossomQueue queue;
	initializeBlossomQueueTestbench(&queue);
	for ( i = 0; i < blossom->childrenBlossomCount && !done; i++)
	{
		enqueueBlossomTestbench(&queue, blossom->childrenBlossoms[i]);
	}
	while (!isBlossomQueueEmptyTestbench(&queue) && !done)
	{
		Blossom* currentBlossom = dequeueBlossomTestbench(&queue);
		for ( i = 0; i < currentBlossom->childrenDefectedNodesCount && !done; i++)
		{
			if (currentBlossom->childrenDefectedNodes[i]->connectedToVirtual)
			{
				//printf("Defected node %d is connected to virtual\n", currentBlossom->childrenDefectedNodes[i]->number);
				position = currentBlossom->childrenDefectedNodes[i]->number;
				modifyInternalConnectionsTestbench(currentBlossom->childrenDefectedNodes[i]->directParent->myself, currentBlossom->childrenDefectedNodes[i]);
				done = true;
			}
		}
		for ( i = 0; i < currentBlossom->childrenBlossomCount && !done; i++)
		{
			enqueueBlossomTestbench(&queue, currentBlossom->childrenBlossoms[i]);
		}
	}
	if (position != -1) {
		pairs[*pairsCount].node1 = defectedNodes[position].y * (d + 1) + defectedNodes[position].x;
		if (defectedNodes[position].x * 2 < d) {
			pairs[*pairsCount].node2 = defectedNodes[position].y * (d + 1);
		}
		else
		{
			pairs[*pairsCount].node2 = defectedNodes[position].y * (d + 1) + d;
		}
		(*pairsCount)++;
	}
	//else
		//DEBUG_PRINT("ERROR during finding the connection in the Blossom for the virtual node\n");
}

void createFinalConnectionsTestbench(short d, PairDefectedNode *pairs,short *pairsCount)
{
	//DEBUG_PRIORITY_PRINT("\n");
	//DEBUG_PRIORITY_PRINT("Final connections\n");
	short usedNodes[MAXDEFECTEDNODES];
	Queue queue;
	short i,j;
	(*pairsCount) = 0;
	for ( i = 0; i < defectedNodesCount; i++)
	{
		if (!defectedNodes[i].usable || defectedNodes[i].hasParent)
			usedNodes[i] = -2;
		else
			usedNodes[i] = -1;
	}
	for ( i = 0; i < defectedNodesCount; i++)
	{
		// eliminate array of pointers
		DefectedNode* defectedNode = &defectedNodes[i];
		short count = 1;
		short isVirtual = -1;
		//Check if the node is not used
		// if the defected node is not used and has no parent then add it to the stack
		// Only top level nodes are considered in the execution
		/*
		if (defectedNode->usable) {
			//DEBUG_PRINT("Check defectedNode %d\n", defectedNode->number);
			//DEBUG_PRINT("Defected node %d has %d touches", defectedNode->number, defectedNode->touchCount);
			//DEBUG_PRINT("with usedNodes %d\n", usedNodes[i]);
		}
		*/
		if (defectedNode->usable && usedNodes[i] == -1 && defectedNode->touchCount < 2)
		{
			//DEBUG_PRINT("Checking node %d\n", defectedNode->number);
			//start from top level node in the chain
			initializeQueueTestbench(&queue);
			enqueueTestbench(&queue, defectedNode);
			DefectedNode* lastNode = defectedNode;
			usedNodes[defectedNode->number] = 0;
			while (!isQueueEmptyTestbench(&queue))
			{
				DefectedNode* currentNode = dequeueTestbench(&queue);
				if (currentNode->connectedToVirtual && checkGoodVirtualTestbench(currentNode)) {
					if (isVirtual == -1)
						isVirtual = currentNode->number;
					else if (defectedNodes[isVirtual].isBlossom)
						//need to symplify the most possible
						isVirtual = currentNode->number;
				}
				//setup for non expansion
				currentNode->expandOrReduce = 0;
				for ( j = 0; j < currentNode->touchCount; j++)
				{
					DefectedNode* nextNode = currentNode->touch[j].node;
					// Ignore the return touch of the next node that is the same as the previous
					if (nextNode != lastNode)
					{
						count++;
						usedNodes[currentNode->touch[j].node->number] = usedNodes[currentNode->number] + 1;
						//print all what was done
						//DEBUG_PRINT("Added node %d to the queue\n", nextNode->number);
						enqueueTestbench(&queue, nextNode);
					}
				}
				lastNode = currentNode;
			}

			if (count % 2 == 1)
			{
				DefectedNode* defectedStrongNode = NULL;
				DefectedNode* defectedWeakNode = NULL;
				//DEBUG_PRINT("Node %d has an odd number of nodes in the chain\n", defectedNode->number);
				//BAD GET VIRTUAL NODE
				//DEBUG_PRINT("THe node %d has %d touches\n", defectedNodes[isVirtual].number, defectedNodes[isVirtual].touchCount);
				if (defectedNodes[isVirtual].touchCount == 2)
				{
					//find the node connected with a WEAK connection and delete the connection
					for ( j = 0; j < defectedNodes[isVirtual].touchCount; j++)
					{
						if (defectedNodes[isVirtual].touch[j].touchType == STRONG)
						{
							//delete the connection
							//DEBUG_PRINT("Deleting strong connection between %d and %d\n", defectedNodes[isVirtual].number, defectedNodes[isVirtual].touch[j].node->number);
							defectedStrongNode = defectedNodes[isVirtual].touch[j].node;
							deleteTouchTestbench(&defectedNodes[isVirtual], defectedNodes[isVirtual].touch[j].node, true);
						}
						else {
							//delete the connection
							//DEBUG_PRINT("Deleting weak connection between %d and %d\n", defectedNodes[isVirtual].number, defectedNodes[isVirtual].touch[j].node->number);
							defectedWeakNode = defectedNodes[isVirtual].touch[j].node;
							deleteTouchTestbench(&defectedNodes[isVirtual], defectedNodes[isVirtual].touch[j].node, true);
						}
					}

				}
				else if (defectedNodes[isVirtual].touchCount == 1) {
					if (defectedNodes[isVirtual].touch[0].touchType == STRONG)
					{
						//delete the connection
						defectedStrongNode = defectedNodes[isVirtual].touch[0].node;
						deleteTouchTestbench(&defectedNodes[isVirtual], defectedNodes[isVirtual].touch[0].node, true);
					}
					else {
						defectedWeakNode = defectedNodes[isVirtual].touch[0].node;
						deleteTouchTestbench(&defectedNodes[isVirtual], defectedNodes[isVirtual].touch[0].node, true);
					}
				}
				if (defectedStrongNode != NULL) {

					//DEBUG_PRINT("The node %d had a strong connection\n", defectedStrongNode->number);
					changeAllInChainTestbench(defectedStrongNode);
					/*
					if (defectedStrongNode->isBlossom)
					{
						//print all touch of its defected children

						for ( short k = 0; k < defectedStrongNode->myself->childrenDefectedNodesCount; k++)
						{
							//DEBUG_PRINT("Defected node %d has touch count %d\n", defectedStrongNode->myself->childrenDefectedNodes[k]->number, defectedStrongNode->myself->childrenDefectedNodes[k]->touchCount);
							for ( j = 0; j < defectedStrongNode->myself->childrenDefectedNodes[k]->touchCount; j++)
							{
								//DEBUG_PRINT("Defected node %d touches %d with %d\n", defectedStrongNode->myself->childrenDefectedNodes[k]->number, defectedStrongNode->myself->childrenDefectedNodes[k]->touch[j].node->number, defectedStrongNode->myself->childrenDefectedNodes[k]->touch[j].touchType);
							}
						}
					}
					*/
					constructionOfPairsTestbench(defectedStrongNode, &queue, pairs, pairsCount, d);
					//use a queue to expand the chain

				}
				if (defectedWeakNode != NULL)
				{
					//DEBUG_PRINT("The node %d had a weak connection\n", defectedWeakNode->number);
					constructionOfPairsTestbench(defectedWeakNode, &queue, pairs, pairsCount, d);
				}
				if (defectedNodes[isVirtual].isBlossom)
				{
					//DEBUG_PRINT("The node connectd to the virtual node is a blossom\n");
					findVirtualInnerNodeTestbench(&defectedNodes[isVirtual], pairs, pairsCount, d);
				}
				else {
					pairs[*pairsCount].node1 = defectedNodes[isVirtual].y * (d + 1) + defectedNodes[isVirtual].x;
					if (defectedNodes[isVirtual].x * 2 < d) {
						pairs[*pairsCount].node2 = defectedNodes[isVirtual].y * (d + 1);
					}
					else
					{
						pairs[*pairsCount].node2 = defectedNodes[isVirtual].y * (d + 1) + d;
					}
					(*pairsCount)++;
				}
				if (defectedNodes[isVirtual].isBlossom)
					findAllBlossomStrongConnectionTestbench(&defectedNodes[isVirtual], pairs, pairsCount, d);
			}
			else {
				//DEBUG_PRINT("Node %d has a stable chain\n", defectedNode->number);
				constructionOfPairsTestbench(defectedNode, &queue, pairs, pairsCount, d);
			}
		}
	}
	printAllElementsTestbench();
}
/*
//TODO check if in order blossom expansion
void doOperationOnBlossom(int operation, DefectedNode* defectedNode, short d)
{
	Blossom* blossom = defectedNode->myself;
	BlossomQueue queue;
	DefectedNode* defected[(MAXELEMENTS * (MAXELEMENTS - 1))] = { NULL };
	short defectedCount = 0;

	initializeBlossomQueueTestbench(&queue);
	enqueueBlossomTestbench(&queue, blossom);
	bool finished = false;
	while (!isBlossomQueueEmptyTestbench(&queue))
	{
		Blossom* currentBlossom = dequeueBlossomTestbench(&queue);
		//put each blossom children in the queue
		for (short i = 0; i < currentBlossom->childrenBlossomCount; i++)
		{
			enqueueBlossomTestbench(&queue, currentBlossom->childrenBlossoms[i]);
		}
		//put each defectedNode children in defectedNodes
		//in inverse order
		for (short i = currentBlossom->childrenDefectedNodesCount - 1; i >= 0; i--)
		{
			defected[defectedCount] = currentBlossom->childrenDefectedNodes[i];
			defectedCount++;
		}

	}
	for (short i = defectedCount - 1; i >= 0; i--)
	{
		//print all defected nodes number
		//DEBUG_PRINT("Defected node %d\n", defected[i]->number);
	}
	//run through all the defected nodes in reverse order and expand or retract them
	for (short i = defectedCount - 1; i >= 0; i--)
	{
		DefectedNode* defNode = defected[i];
		if (operation == 1)
		{
			processExpandDefectedNodesTestbench(d, defNode);
		}
		else {
			processRetractDefectedNodesTestbench(d, defNode);
		}
	}
	defectedNode->lastRoundModified = actualRound;
	if (operation == 1)
	{
		blossom->dimension++;
	}
	else {
		blossom->dimension--;
	}

}*/
void resolveTestBench(short nDefNodes, short d, short defectedN[], PairDefectedNode pairs[], short* pairsCount) {
	//FOR TEST PORPUSES DIMENSION IS 7
	//
	// 
	// checkDefNodesTestbench(defectedNodes, blossoms, 0, &defectedNodesCount, &blossomsCount);
	//
	// 
	// 
	// print all input
	//DEBUG_ALWAYS_PRINT("Defected nodes count: %d\n", nDefNodes);
	//DEBUG_ALWAYS_PRINT("Dimension: %d\n", d);
	short i;
	for ( i = 0; i < nDefNodes; i++)
	{
		//DEBUG_ALWAYS_PRINT("%d ", defectedN[i]);
	}
	//DEBUG_ALWAYS_PRINT("\n");
	defectedNodesCount = nDefNodes;
	numberOfEdges = d * d + (d - 1) * (d - 1);
	// Run the program with the given d and defectedNodes
	if (!checkNodesTestbench(defectedN, d))
		DEBUG_PRINT("Invalid defected nodes\n");
	//initilize the defected nodes
	for ( i = 0; i < MAXELEMENTS; i++)
	{
		for (short j = 0; j < MAXELEMENTS + 1; j++)
		{
			graph[i][j].number = i * (d + 1) + j;
			graph[i][j].weightGot = 0;
			graph[i][j].defectedparent = NULL;
			graph[i][j].blossomParent = NULL;
			graph[i][j].y = i;
			graph[i][j].x = j;
			//NEED TO DELETE = NULL TO CHECK FOR THE ERROR
			if (j == 0 || j == d)
			{
				graph[i][j].isVirtual = true;
			}
			else
			{
				graph[i][j].isVirtual = false;
			}
			//initialize the edges
			if (j != d)
			{
				graph[i][j].edges[EAST] = &edges[i * d + j];
			}
			else
			{
				graph[i][j].edges[EAST] = NULL;
			}
			if (j != 0)
			{
				graph[i][j].edges[WEST] = &edges[i * d + j - 1];
			}
			else
			{
				graph[i][j].edges[WEST] = NULL;
			}
			if (i != d - 1 && j != 0 && j != d)
			{
				graph[i][j].edges[SOUTH] = &edges[d * d + i * (d - 1) + j - 1];
			}
			else
			{
				graph[i][j].edges[SOUTH] = NULL;
			}
			if (i != 0 && j != 0 && j != d)
			{
				graph[i][j].edges[NORTH] = &edges[d * d + (i - 1) * (d - 1) + j - 1];
			}
			else
			{
				graph[i][j].edges[NORTH] = NULL;
			}

		}
	};
	for ( i = 0; i < defectedNodesCount; i++)
	{
		defectedNodes[i].x = defectedN[i] % (d + 1);
		defectedNodes[i].y = defectedN[i] / (d + 1);
		defectedNodes[i].connectedToVirtual = false;
		defectedNodes[i].expandOrReduce = 1;
		defectedNodes[i].hasParent = false;
		defectedNodes[i].isBlossom = false;
		defectedNodes[i].nodesCount = 1;
		defectedNodes[i].nodes[0] = &graph[defectedNodes[i].y][defectedNodes[i].x];
		defectedNodes[i].number = i;
		defectedNodes[i].touchCount = 0;
		defectedNodes[i].canTouchCount = 0;
		defectedNodes[i].lastRoundModified = 0;
		defectedNodes[i].highestParent = NULL;
		defectedNodes[i].directParent = NULL;
		defectedNodes[i].usable = true;
		graph[defectedNodes[i].y][defectedNodes[i].x].defectedparent = &defectedNodes[i];
		graph[defectedNodes[i].y][defectedNodes[i].x].blossomParent = &defectedNodes[i];

	}
	//initilize the edges to weight 1000
	for ( i = 0; i < numberOfEdges; i++)
	{
		edges[i] = (Edge){ i,MAXWEIGHT,0 };
	}

	bool finished = false;
	while (!finished)
	{
		actualRound++;
		//DEBUG_PRINT("\n");
		//DEBUG_PRINT("actualRound %d\n", actualRound);
		//DEBUG_PRINT("\n");
		for ( i = 0; i < defectedNodesCount; i++)
		{
			DefectedNode* defectedNode = &defectedNodes[i];

			if (defectedNode->usable && !defectedNode->isBlossom && defectedNode->lastRoundModified < actualRound)
			{
				if (!defectedNode->hasParent && defectedNode->expandOrReduce == -1)
				{
					//DEBUG_PRINT("\nReducing defected node %d\n", defectedNodes[i].number);
					processRetractDefectedNodesTestbench(d, defectedNode);
					defectedNode->lastRoundModified = actualRound;
				}
				else if (defectedNode->hasParent && defectedNode->highestParent->myself->expandOrReduce == -1)
				{
					//DEBUG_PRINT("\nReducing defected node %d with parent %d\n", defectedNodes[i].number, defectedNode->highestParent->myself->number);
					processRetractDefectedNodesTestbench(d, defectedNode);
					defectedNode->lastRoundModified = actualRound;
					if (defectedNode->highestParent->myself->lastRoundModified < actualRound)
					{
						defectedNode->highestParent->myself->lastRoundModified = actualRound;
						defectedNode->highestParent->dimension--;
					}
				}
			}
		}
		for ( i = 0; i < defectedNodesCount; i++)
		{
			DefectedNode* defectedNode = &defectedNodes[i];
			if (defectedNode->usable && !defectedNode->isBlossom && defectedNode->lastRoundModified < actualRound)
			{
				if (!defectedNode->hasParent && defectedNode->expandOrReduce == 1)
				{
					//DEBUG_PRINT("\n Expanding defected node %d\n", defectedNodes[i].number);
					processExpandDefectedNodesTestbench(d, defectedNode);
					defectedNode->lastRoundModified = actualRound;
				}
				else if (defectedNode->hasParent && defectedNode->highestParent->myself->expandOrReduce == 1)
				{
					//DEBUG_PRINT("\nExpanding defected node %d with parent %d\n", defectedNodes[i].number, defectedNode->highestParent->myself->number);
					processExpandDefectedNodesTestbench(d, defectedNode);
					defectedNode->lastRoundModified = actualRound;
					if (defectedNode->highestParent->myself->lastRoundModified < actualRound)
					{
						defectedNode->highestParent->myself->lastRoundModified = actualRound;
						defectedNode->highestParent->dimension++;
					}
				}
			}
		}
		if (!roundcheckWithQueueTestbench()) //finished if all connected
		{
			//DEBUG_PRINT("All nodes connected\n");
			createFinalConnectionsTestbench(d, pairs, pairsCount);
			finished = true;
		}
	}
	// TODO: Implement the program logic here

	//printf("actualRound %d completed.\n", i + 1);
	nDefNodes = 0;
	//reset all the counts
	blossomsCount = 0;
	defectedNodesCount = 0;
	numberOfEdges = 0;
	actualRound = 0;
}


int main()
{
	short defectedN[2*MAXELEMENTS];
	hlsint def[2*MAXELEMENTS];
	bool error=false;
	FILE* inputFile = fopen(input, "r");
	if (inputFile == NULL) {
		printf("Failed to open the input file.\n");
		return 1;
	}
	printf("File opened successfully.\n");

	int rounds;
	fscanf(inputFile, "%d", &rounds);
	for (int i = 0; i < rounds; i++) {
		int d;
		fscanf(inputFile, "%d", &d);
		hlsint dist=(hlsint)d;
		short nDefNodes = 0;
		char c;
		int val = 0;
		//read until the end of the line
		//the line is number space number \n read all the numbers until the end of the line
		do{
		c = fgetc(inputFile);
		}while(c!='\n');
		c = fgetc(inputFile);
		while (c != '\n')
		{
			if (c == ' ')
			{
				defectedN[nDefNodes] = val;
				val = 0;
				nDefNodes++;
			}
			else if(c!='\r')
			{
				val = val * 10 + (c - '0');
			}
			c = fgetc(inputFile);
		}
		printf("actualRound %d\n", i + 1);


		printf("d: %d\n", d);
		printf("Defected nodes: ");



		for (int j = 0; j < nDefNodes; j++)
		{
			printf("%d ", defectedN[j]);
			def[j]=(hlsint) defectedN[j];
			hlsint bc=def[j]%(dist+1);
			printf("%d ", def[j]);
			printf("%d ", bc);
		}

		printf("\n");
		PairDefectedNode pairs[2*MAXELEMENTS];
		short pairsCount = 0;
		PairDefectedNode pairs1[2*MAXELEMENTS];
		short pairsCount1 = 0;
		hlsint nDef=(hlsint)nDefNodes;
		hlsint pairsC=0;
		resolveTestBench(nDefNodes, d, defectedN, pairs1, &pairsCount1);
		resolve(nDef, dist, def, pairs, &pairsC);
		firstDefectedNodesCount = nDefNodes;
		pairsCount=(short)pairsC;
		for(int f=0;f<pairsCount;f++){
			printf("first %d second %d\n",pairs[f].node1,pairs[f].node2);
			printf("Testbench first %d second %d\n",pairs1[f].node1,pairs1[f].node2);
		}
		if(pairsCount1!=pairsCount){
			printf("NOT SAME COUNT\n");
			printf("TestBench: %d, physical: %d\n",pairsCount1,pairsCount);
			error=true;
		}
		for(short b;b<pairsCount;b++){
			if(pairs[b].node1!=pairs1[b].node1){
				printf("NOT SAME NODE1\n");
				printf("TestBench: %d, physical: %d\n",pairs1[b].node1,pairs[b].node1);
				error=true;
			}
			if(pairs[b].node2!=pairs1[b].node2){
				printf("NOT SAME NODE2\n");
				printf("TestBench: %d, physical: %d\n",pairs1[b].node2,pairs[b].node2);
				error=true;
			}
		}
		//print all pairs
		for (short h = 0; h < pairsCount; h++)
		{
			printf("Pair %d %d\n", pairs[h].node1, pairs[h].node2);
			bool found = false;
			for (short l = 0; l < firstDefectedNodesCount; l++)
			{
				if (pairs[h].node1 == defectedN[l])
				{
					defectedN[l] = -1;
					found = true;
					break;
				}
			}
			//if not found or the number correspond to a virtual node
			if (!found && pairs[h].node1 % (d + 1) != d && pairs[h].node1 % (d + 1) != 0)
			{
				printf("ERROR first node not found and is not virtual\n");
				error=true;
			}

			found = false;
			for (short m = 0; m < firstDefectedNodesCount; m++)
			{
				if (pairs[h].node2 == defectedN[m])
				{
					defectedN[m] = -1;
					found = true;
					break;
				}
			}
			if (!found && pairs[h].node2 % (d + 1) != d && pairs[h].node2 % (d + 1) != 0)
			{
				printf("ERROR second node not found and is not virtual\n");
				error=true;
			}
		}
		for (short k = 0; k < firstDefectedNodesCount; k++)
		{
			if (defectedN[k] != -1)
			{
				printf("ERROR missing value %d\n",defectedN[k]);
				error=true;
			}
		}

		val = 0;

	}

	fclose(inputFile);
	if(error)
		return 1;
	return 0;
}