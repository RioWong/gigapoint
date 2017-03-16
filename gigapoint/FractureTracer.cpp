//##########################################################################
//#                                                                        #
//#                    CLOUDCOMPARE PLUGIN: ccCompass                      #
//#                                                                        #
//#  This program is free software; you can redistribute it and/or modify  #
//#  it under the terms of the GNU General Public License as published by  #
//#  the Free Software Foundation; version 2 of the License.               #
//#                                                                        #
//#  This program is distributed in the hope that it will be useful,       #
//#  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
//#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
//#  GNU General Public License for more details.                          #
//#                                                                        #
//#                     COPYRIGHT: Sam Thiele  2017                        #
//#                                                                        #
//##########################################################################

#include "omega.h"

#include "FractureTracer.h"
#include "PointCloud.h"
#include "iostream"

namespace gigapoint {



FractureTracer::FractureTracer(PointCloud* cloud) : m_relMarkerScale(5.0f),m_cloud(NULL),m_previous(-1) {
    m_cloud = cloud; //store pointer ourselves also
    //m_search_r = calculateOptimumSearchRadius(); //estimate the search radius we want to use    
    m_search_r = m_cloud->getPCInfo()->spacing*32; //should corespond to hierarchy level 4
    std::cout << " Fracture tracer search radius/spacing : " << m_search_r <<" / " << m_cloud->getPCInfo()->spacing << std::endl;
	//store these info as object attributes
	//object->hasMetaData("search_r") && object->hasMetaData("cost_function");
    /*
    QVariantMap* map = new QVariantMap();
	map->insert("search_r", m_search_r); 
	QString cost_function = "";
	if (COST_MODE & MODE::RGB)
		cost_function += "RGB,";
	if (COST_MODE & MODE::DARK)
		cost_function += "Dark,";
	if (COST_MODE & MODE::LIGHT)
		cost_function += "Light,";
	if (COST_MODE & MODE::CURVE)
		cost_function += "Curve,";
	if (COST_MODE & MODE::GRADIENT)
		cost_function += "Grad,";
	if (COST_MODE & MODE::DISTANCE)
		cost_function += "Dist,";
	if (COST_MODE & MODE::SCALAR)
		cost_function += "Scalar,";
	if (COST_MODE & MODE::INV_SCALAR)
		cost_function += "Inv_Scalar,";
	cost_function = cost_function.remove(cost_function.size() - 1, 1); //remove trailing comma
    map->insert("cost_function", cost_function);
    setMetaData(*map, true);
    */

}

int FractureTracer::insertWaypoint(Point Q)
{

	if (m_waypoints.size() >= 2)
	{
		//get location of point to add
		//const CCVector3* Q = m_cloud->getPoint(pointId);
        //float Q[3] = *m_cloud->getPoint(pointId);
        Point start, end;
		//check if point is "inside" any segments
		for (int i = 1; i < m_waypoints.size(); i++)
		{
			//get start and end points
            //m_cloud->getPoint(m_waypoints[i - 1], start);
            //m_cloud->getPoint(m_waypoints[i], end);
            start=m_waypoints[i-1];
            end=m_waypoints[i];

			//are we are "inside" this segment
            if (Utils::inCircle(start.position, end.position, Q.position))
			{
				//insert waypoint
                m_waypoints.insert(m_waypoints.begin() + i, Q);
				m_previous = i;
				return i;
			}
		}

		//check if the point is closer to the start than the end -> i.e. the point should be 'pre-pended'
        //CCVector3 sp = Q - start;
        //CCVector3 ep = Q - end;
        float ds=DIST3(Q.position,start.position);
        float de=DIST3(Q.position,end.position);
        if (ds < de)
		{
            m_waypoints.insert(m_waypoints.begin(), Q);
			m_previous = 0;
			return 0;
		}
	}

	//add point to end of the trace
    m_waypoints.push_back(Q);
	m_previous = static_cast<int>(m_waypoints.size()) - 1;
	return m_previous;
}


bool FractureTracer::optimizePath(int maxIterations)
{
	bool success = true;

	if (m_waypoints.size() < 2)
	{
		m_trace.clear();
		return false; //no segments...
	}

	#ifdef DEBUG_PATH
	int idx = m_cloud->getScalarFieldIndexByName("Search"); //look for scalar field to write search to
	if (idx == -1) //doesn't exist - create
		idx=m_cloud->addScalarField("Search");
	m_cloud->setCurrentScalarField(idx);
	#endif
	
	//loop through segments and build/rebuild trace
    Point start, end;
    int tID; //declare vars
	for (unsigned i = 1; i < m_waypoints.size(); i++)
	{
		//calculate indices
		start = m_waypoints[i - 1]; //global point id for the start waypoint
		end = m_waypoints[i]; //global point id for the end waypoint
		tID = i - 1; //id of the trace segment id (in m_trace vector)

		//are we adding to the end of the trace?
		if (tID >= m_trace.size()) 
		{
            std::deque<Point> segment = optimizeSegment(start, end, m_search_r, maxIterations); //calculate segment
			m_trace.push_back(segment); //store segment
			success = success && !segment.empty(); //if the queue is empty, we failed
		} else //no... we're somewhere in the middle - update segment if necessary
		{
			if (!m_trace[tID].empty() && (m_trace[tID][0] == start) &&  (m_trace[tID][m_trace[tID].size() - 1] == end)) //valid trace and start/end match
				continue; //this trace has already been calculated - we can skip! :)
			else
			{
				//calculate segment
                std::deque<Point> segment = optimizeSegment(start, end, m_search_r, maxIterations); //calculate segment
				success = success && !segment.empty(); //if the queue is empty, we failed

				//add trace
				if (m_trace[tID][m_trace[tID].size() - 1] == end) //end matches - we can replace the current trace & things should be sweet (all prior traces will have been updated already)
					m_trace[tID] = segment; //end is correct - overwrite this block, then hopefully we will match in the next one
				else //end doesn't match - we need to insert
					m_trace.insert(m_trace.begin()+tID, segment);
			}
		}
	}

	#ifdef DEBUG_PATH
	CCLib::ScalarField * f = m_cloud->getScalarField(idx);
	f->computeMinAndMax();
	#endif

	return success;
}

/*
void FractureTracer::finalizePath()
{
    //clear existing points in background "polyline"
    clear();

    //push trace buffer to said polyline (for save/export etc.)
    for (std::deque<int> seg : m_trace)
    {
        for (int p : seg)
        {
            addPointIndex(p);
        }
    }
}
*/

//int FractureTracer::COST_MODE = FractureTracer::MODE::DARK; //set default cost mode
std::deque<Point> FractureTracer::optimizeSegment(Point start, Point end, float search_r, int maxIterations, int offset)
{

	//check handle to point cloud
    if (!m_cloud)
	{
        return std::deque<Point>(); //error -> no cloud
	}

	//retreive and store start & end rgb
    m_start_rgb= start;
    m_end_rgb= end;
    //const ColorCompType* s = m_cloud->getPointColor(start);
    //const ColorCompType* e = m_cloud->getPointColor(end);
    //m_start_rgb[0] = s[0]; m_start_rgb[1] = s[1]; m_start_rgb[2] = s[2];
    //m_end_rgb[0] = e[0]; m_end_rgb[1] = e[1]; m_end_rgb[2] = e[2];


	//get location of target node - used to optimise algorithm to stop searching paths leading away from the target
    //const CCVector3* end_v = m_root->getPoint(end);

	//code essentialy taken from wikipedia page for Djikstra: https://en.wikipedia.org/wiki/Dijkstra%27s_algorithm
    std::map<PointIndex, PointIndex> closedSet; //visited nodes (key=nodeID, value=prevNodeID)
    std::map<PointIndex, PointIndex> openSet; //nodes we are currently exploring (key=nodeID, value=prevNodeID)
    std::map<PointIndex, int> dist; //<node, cost estimate from start to end via this node>

	//declare variables used in the loop
    Point current;

	int cost = 0;
	int smallest_cost = 999999;
	int iter_count = 0;
	float cur_d2, next_d2;

	//setup octree & values for nearest neighbour searches
    //ccOctree::Shared oct = m_cloud->getOctree();
    //if (!oct)
    //{
    //	oct = m_cloud->computeOctree(); //if the user clicked "no" when asked to compute the octree then tough....
    //}
    //unsigned char level = oct->findBestLevelForAGivenNeighbourhoodSizeExtraction(search_r);

    openSet[start.index] = start.index; //open start node
    dist[start.index] = 0; //start-start distance = 0
	while (openSet.size() > 0) //while unvisited nodes exist
	{
		//check if we excede max iterations
        if (iter_count > maxIterations) {
            cout << "[FractureTracer::optimizeSegment] max iterations reached. Stopping" << std::endl;
            return std::deque<Point>(); //bail
        }
		iter_count++;

		//get node from openSet with smallest cost
		smallest_cost = 999999;
        for (std::map<PointIndex, PointIndex>::iterator it=openSet.begin();it!=openSet.end();++it){
        //for (const auto &myPair : openSet) {
            cost = dist[(*it).first];
			if (cost < smallest_cost)
			{
				smallest_cost = cost;
                current =m_cloud->getPointFromIndex((*it).first);
			}
		}

        closedSet[current.index] = openSet[current.index]; //add current to closedSet
        openSet.erase(current.index); //remove current from openSet
				
        if (current.index == end.index) //we've found it!
		{
            std::deque<Point> path;
			path.push_back(end); //add end node

			//traverse backwards to reconstruct path
            current = m_cloud->getPointFromIndex(closedSet[current.index]); //move back one
            while (current.index != start.index)
			{
                path.push_front(current);
                current =m_cloud->getPointFromIndex(closedSet[current.index]);
			}
			path.push_front(start);

			//return
            cout << "[FractureTracer::optimizeSegment] iterations:" << iter_count << std::endl;
			return path;
		}

		//calculate distance from current nodes parent to end -> avoid going backwards (in euclidean space) [essentially stops fracture turning > 90 degrees)
        Point cur = current;//m_cloud->getPoint(closedSet[current]);
        cur_d2 =	(cur.position[0] - end.position[0])*(cur.position[0] - end.position[0]) +
                    (cur.position[1] - end.position[1])*(cur.position[1] - end.position[1]) +
                    (cur.position[2] - end.position[2])*(cur.position[2] - end.position[2]);

		//fill "neighbours" with nodes - essentially get results of a "sphere" search around active current point
        std::vector< Point >neighbours=m_cloud->getPointsInSphericalNeighbourhood(current, search_r);
        cout << "[FractureTracer::optimizeSegment] getPointsInSphericalNeighbourhood returned :" << neighbours.size() << std::endl;
		//loop through neighbours
        for (size_t i = 0; i < neighbours.size(); i++) //N.B. i = [pointID,cost]
		{
            Point apoint = neighbours[i];

			//calculate (squared) distance from this neighbour to the end
            next_d2 =	(apoint.position[0] - end.position[0])*(apoint.position[0] - end.position[0]) +
                        (apoint.position[1] - end.position[1])*(apoint.position[1] - end.position[1]) +
                        (apoint.position[2] - end.position[2])*(apoint.position[2] - end.position[2]);

			if (next_d2 >= cur_d2) //Bigger than the original distance? If so then bail.
				continue;

            if (closedSet.count(apoint.index) == 1) //Is point in closed set? If so, bail.
				continue;

			//calculate cost to this neighbour
            cost = getSegmentCostRGB(current, apoint);

			#ifdef DEBUG_PATH
            m_cloud->setPointScalarValue(apoint.index, static_cast<ScalarType>(cost)); //STORE VISITED NODES (AND COST) FOR DEBUG VISUALISATIONS
			#endif

			//transform into cost from start node
            cost += dist[current.index];

			//have we visited this node before?
            if (dist.count(apoint.index) == 1)
			{
				//is this cost better?
                if (cost < dist[apoint.index])
				{

                    dist[apoint.index] = cost; //update cost with better value
                    closedSet[apoint.index] = current.index; //store this as the best path to this node
				}
				else
					continue; //skip
			} else //this is the first time we've looked at this node
			{
                openSet[apoint.index] = current.index; //add to open set - we can move here next
                dist[apoint.index] = cost; //store cost
			}
		}
	}
    cout << "[FractureTracer::optimizeSegment] iterations:" << iter_count << std::endl;
	assert(false);
    return std::deque<Point>(); //shouldn't come here?

}

int FractureTracer::getSegmentCostRGB(Point p1_rgb, Point p2_rgb)
{
    //get colors
    //const ColorCompType* p1_rgb = m_cloud->getPointColor(p1);
    //const ColorCompType* p2_rgb = m_cloud->getPointColor(p2);

    //cost function: cost = |c1-c2| + 0.25 ( |c1-start| + |c1-end| + |c2-start| + |c2-end| )
    //IDEA: can we somehow optimize all the square roots here (for speed)?
    return sqrt(
        //|c1-c2|
        (p1_rgb.color[0] - p2_rgb.color[0]) * (p1_rgb.color[0] - p2_rgb.color[0]) +
        (p1_rgb.color[1] - p2_rgb.color[1]) * (p1_rgb.color[1] - p2_rgb.color[1]) +
        (p1_rgb.color[2] - p2_rgb.color[2]) * (p1_rgb.color[2] - p2_rgb.color[2])) + 0.25 * (
        //|c1-start|
        sqrt((p1_rgb.color[0] - m_start_rgb.color[0]) * (p1_rgb.color[0] - m_start_rgb.color[0]) +
        (p1_rgb.color[1] - m_start_rgb.color[1]) * (p1_rgb.color[1] - m_start_rgb.color[1]) +
        (p1_rgb.color[2] - m_start_rgb.color[2]) * (p1_rgb.color[2] - m_start_rgb.color[2])) +
        //|c1-end|
        sqrt((p1_rgb.color[0] - m_end_rgb.color[0]) * (p1_rgb.color[0] - m_end_rgb.color[0]) +
        (p1_rgb.color[1] - m_end_rgb.color[1]) * (p1_rgb.color[1] - m_end_rgb.color[1]) +
        (p1_rgb.color[2] - m_end_rgb.color[2]) * (p1_rgb.color[2] - m_end_rgb.color[2])) +
        //|c2-start|
        sqrt((p2_rgb.color[0] - m_start_rgb.color[0]) * (p2_rgb.color[0] - m_start_rgb.color[0]) +
        (p2_rgb.color[1] - m_start_rgb.color[1]) * (p2_rgb.color[1] - m_start_rgb.color[1]) +
        (p2_rgb.color[2] - m_start_rgb.color[2]) * (p2_rgb.color[2] - m_start_rgb.color[2])) +
        //|c2-end|
        sqrt((p2_rgb.color[0] - m_end_rgb.color[0]) * (p2_rgb.color[0] - m_end_rgb.color[0]) +
        (p2_rgb.color[1] - m_end_rgb.color[1]) * (p2_rgb.color[1] - m_end_rgb.color[1]) +
        (p2_rgb.color[2] - m_end_rgb.color[2]) * (p2_rgb.color[2] - m_end_rgb.color[2]))) / 3.5; //N.B. the divide by 3.5 scales this cost function to range between 0 & 255
}


# if 0
int FractureTracer::getSegmentCost(int p1, int p2, float search_r)
{
	int cost=1; //n.b. default value is 1 so that if no cost functions are used, the function doesn't crash (and returns the unweighted shortest path)
	if (m_cloud->hasColors()) //check cloud has colour data
	{
		if (COST_MODE & MODE::RGB)
			cost += getSegmentCostRGB(p1, p2);
		if (COST_MODE & MODE::DARK)
			cost += getSegmentCostDark(p1, p2);
		if (COST_MODE & MODE::LIGHT)
			cost += getSegmentCostLight(p1, p2);
		if (COST_MODE & MODE::GRADIENT)
			cost += getSegmentCostGrad(p1, p2, search_r);
	}
	if (m_cloud->hasDisplayedScalarField()) //check cloud has scalar field data
	{
		if (COST_MODE & MODE::SCALAR)
			cost += getSegmentCostScalar(p1, p2);
		if (COST_MODE & MODE::INV_SCALAR)
			cost += getSegmentCostScalarInv(p1, p2);
	}

	//these cost functions can be used regardless
	if (COST_MODE & MODE::CURVE)
		cost += getSegmentCostCurve(p1, p2);
	if (COST_MODE & MODE::DISTANCE)
		cost += getSegmentCostDist(p1, p2);

	return cost;
}



int FractureTracer::getSegmentCostDark(int p1, int p2)
{
	//return magnitude of the point p2
	//const ColorCompType* p1_rgb = m_cloud->getPointColor(p1);
	const ColorCompType* p2_rgb = m_cloud->getPointColor(p2);

	//return "taxi-cab" length
	return (p2_rgb[0] + p2_rgb[1] + p2_rgb[2]); //note: this will naturally give a maximum of 765 (=255 + 255 + 255)

	//return length
	//return sqrt((p2_rgb[0] * p2_rgb[0]) +
	//	(p2_rgb[1] * p2_rgb[1]) +
	//	(p2_rgb[2] * p2_rgb[2]))*2; //*2 makes it add to 884 for, which is a prereq for cost function mixing
}

int FractureTracer::getSegmentCostLight(int p1, int p2)
{
	//return the opposite of getCostDark
	return 765 - getSegmentCostDark(p1, p2);
}

int FractureTracer::getSegmentCostCurve(int p1, int p2)
{
	//put neighbourhood in a CCLib::Neighbourhood structure
	if (m_neighbours.size() > 4) //need at least 4 points to calculate curvature....
	{
		m_neighbours.push_back(m_p); //add center point onto end of neighbourhood

		//compute curvature
		CCLib::DgmOctreeReferenceCloud nCloud(&m_neighbours, static_cast<unsigned>(m_neighbours.size()));
		CCLib::Neighbourhood Z(&nCloud);
		float c = Z.computeCurvature(0, CCLib::Neighbourhood::CC_CURVATURE_TYPE::MEAN_CURV);
		
		m_neighbours.erase(m_neighbours.end()-1); //remove center point from neighbourhood (so as not to screw up loops)

		//curvature tends to range between 0 (high cost) and 10 (low cost), though it can be greater than 10 in extreme cases
		//hence we need to map to domain 0 - 10 and then transform that to the (integer) domain 0 - 884 to meet the cost function spec
		if (c > 10)
			c = 10;

		//scale curvature to range 0, 765
		c *= 76.5;

		//note that high curvature = low cost, hence subtract curvature from 765
		return 765 - c;
		
	}
	return 765; //unknown curvature - this point is high cost.
}

int FractureTracer::getSegmentCostGrad(int p1, int p2, float search_r)
{
	CCVector3 p = *m_cloud->getPoint(p2);
	const ColorCompType* p2_rgb = m_cloud->getPointColor(p2);
	int p_value = p2_rgb[0] + p2_rgb[1] + p2_rgb[2];

	if (m_neighbours.size() > 2) //need at least 2 points to calculate gradient....
	{
		//N.B. The following code is mostly stolen from the computeGradient function in CloudCompare
		CCVector3d sum(0, 0, 0);
		CCLib::DgmOctree::PointDescriptor n;
		for (int i = 0; i < m_neighbours.size(); i++)
		{
			n = m_neighbours[i];

			//vector from p1 to m_p
			CCVector3 deltaPos = *n.point - p;
			double norm2 = deltaPos.norm2d();

			//colour
			const ColorCompType* c = m_cloud->getPointColor(n.pointIndex);
			int c_value = c[0] + c[1] + c[2];

			//calculate gradient weighted by distance to the point (i.e. divide by distance^2)
			if (norm2 > ZERO_TOLERANCE)
			{
				//color magnitude difference
				int deltaValue = p_value - c_value;
				//divide by norm^2 to get distance-weighted gradient
				deltaValue /= norm2; //we divide by 'norm' to get the normalized direction, and by 'norm' again to get the gradient (hence we use the squared norm)
				//sum stuff
				sum.x += deltaPos.x * deltaValue; //warning: here 'deltaValue'= deltaValue / squaredNorm(deltaPos) ;)
				sum.y += deltaPos.y * deltaValue;
				sum.z += deltaPos.z * deltaValue;
			}
		}

		float gradient = sum.norm() / m_neighbours.size();

		//ensure gradient is lass than a case-specific maximum gradient (colour change from white to black across a distance or search_r,
		//                                                                                  giving a gradient of (255+255+255) / search_r)
		gradient = std::min(gradient, 765 / search_r);
		gradient *= search_r; //scale between 0 and 765
		return 765 - gradient; //return inverse gradient (high gradient = low cost)
	}
	return 765; //no gradient = high cost
}

int FractureTracer::getSegmentCostDist(int p1, int p2)
{
	return 255;
}

int FractureTracer::getSegmentCostScalar(int p1, int p2)
{
    //m_cloud->getCurrentDisplayedScalarFieldIndex();
	ccScalarField* sf = static_cast<ccScalarField*>(m_cloud->getCurrentDisplayedScalarField());
	return (sf->getValue(p2)-sf->getMin()) * (765 / (sf->getMax()-sf->getMin())); //return scalar field value mapped to range 0 - 765
}

int FractureTracer::getSegmentCostScalarInv(int p1, int p2)
{
	ccScalarField* sf = static_cast<ccScalarField*>(m_cloud->getCurrentDisplayedScalarField());
	return (sf->getMax() - sf->getValue(p2)) * (765 / (sf->getMax() - sf->getMin())); //return inverted scalar field value mapped to range 0 - 765
}
#endif
#if TODO
ccPlane* FractureTracer::fitPlane(int surface_effect_tolerance, float min_planarity)
{
	//put all "trace" points into the cloud
	finalizePath();
	
	if (size() < 3)
		return 0; //need three points to fit a plane

	//check we are not trying to fit a plane to a line
	CCLib::Neighbourhood Z(this);

	//calculate eigenvalues of neighbourhood
	CCLib::SquareMatrixd cov = Z.computeCovarianceMatrix();
	CCLib::SquareMatrixd eigVectors; std::vector<double> eigValues;
	if (Jacobi<double>::ComputeEigenValuesAndVectors(cov, eigVectors, eigValues, true))
	{
		//sort eigenvalues into decending order
		std::sort(eigValues.rbegin(), eigValues.rend());

		float y = eigValues[1]; //middle eigen
		float z = eigValues[2]; //smallest eigen (parallel to plane normal)

		//calculate planarity (0 = line or random, 1 = plane)
		float planarity = 1.0f - z / y;
		if (planarity < min_planarity)
		{
			return 0;
		}
	}

	//fit plane
	double rms = 0.0; //output for rms
	ccPlane* p = ccPlane::Fit(this, &rms);
    
	//calculate and store plane attributes
	//get plane normal vector
	CCVector3 N(p->getNormal());
	//We always consider the normal with a positive 'Z' by default!
	if (N.z < 0.0)
		N *= -1.0;

	//calculate dip/dip direction
	float strike, dip;
	ccNormalVectors::ConvertNormalToStrikeAndDip(N, strike, dip);
	//QString dipAndDipDirStr = ccNormalVectors::ConvertStrikeAndDipToString(s, d);
	QString dipAndDipDirStr = QString("%1/%2").arg((int)strike, 3, 10, QChar('0')).arg((int)dip, 2, 10, QChar('0'));
	p->setName(dipAndDipDirStr);
	//calculate centroid
	CCVector3 C = p->getCenter();

	//store attributes (centroid, strike, dip, RMS) on plane
	QVariantMap* map = new QVariantMap();
	map->insert("Cx", C.x); map->insert("Cy", C.y); map->insert("Cz", C.z); //centroid
	map->insert("Nx", N.x); map->insert("Ny", N.y); map->insert("Nz", N.z); //normal
	map->insert("Strike", strike); map->insert("Dip", dip); //strike & dip
	map->insert("RMS", rms); //rms
	map->insert("Radius", m_search_r); //search radius
	p->setMetaData(*map, true);

	//test for 'surface effect'
	if (m_cloud->hasNormals())
	{
		//calculate average normal of points on trace
		CCVector3 n_avg;
		for (unsigned i = 0; i < size(); i++)
		{
			//get normal vector
			CCVector3 n = ccNormalVectors::GetNormal(m_cloud->getPointNormalIndex(this->getPointGlobalIndex(i)));
			n_avg += n;
		}
		n_avg *= (PC_ONE / size()); //turn sum into average


		//compare to plane normal
		CCVector3 n = p->getNormal();
		if (acos(n_avg.dot(n)) < 0.01745329251*surface_effect_tolerance) //0.01745329251 converts deg to rad
		{
			//this is a false (surface) plane - reject
			return 0; //don't return plane
		}
	}

	//all is good! Return the plane :)
	return p;
}
#endif

/* replaced by pcinfo spacing devided by hierarchy level
float FractureTracer::calculateOptimumSearchRadius()
{
#if TODO
	CCLib::DgmOctree::NeighboursSet neighbours;

	//setup octree & values for nearest neighbour searches
	ccOctree::Shared oct = m_cloud->getOctree();
	if (!oct)
	{
		oct = m_cloud->computeOctree(); //if the user clicked "no" when asked to compute the octree then tough....
	}

	//init vars needed for nearest neighbour search
	unsigned char level = oct->findBestLevelForAGivenPopulationPerCell(2);
	CCLib::ReferenceCloud* nCloud = new  CCLib::ReferenceCloud(m_cloud);

	//pick 15 random points
	int r;
	unsigned int npoints = m_cloud->size();
	double d,dsum=0;
	srand(npoints); //set seed as n for repeatability
	for (unsigned int i = 0; i < 30; i++)
	{
		//int rn = rand() * rand(); //need to make bigger than rand max...
		r = (rand()*rand()) % npoints; //random(ish) number between 0 and n

		//find nearest neighbour for point
		nCloud->clear(false);
		int n = oct->findPointNeighbourhood(m_cloud->getPoint(r), nCloud, 2, level, d);

		if (d != -1) //if a point was found
		{
			dsum += sqrt(d);
		}
	}
	
	//average nearest-neighbour distances
	d = dsum / 30;
	
	//return a number slightly larger than the average distance
	return d*1.5;
#endif
    return 0;
}
*/

//static QSharedPointer<ccSphere> c_unitPointMarker(0);
/*
void FractureTracer::drawMeOnly(CC_DRAW_CONTEXT& context)
{
	if (!MACRO_Foreground(context)) //2D foreground only
		return; //do nothing

	if (MACRO_Draw3D(context))
	{
		if (m_waypoints.size() == 0) //no points -> bail!
			return;

		//get the set of OpenGL functions (version 2.1)
		QOpenGLFunctions_2_1 *glFunc = context.glFunctions<QOpenGLFunctions_2_1>();
		if (glFunc == nullptr) {
			assert(false);
			return;
		}

		//check sphere exists
		if (!c_unitPointMarker)
		{
			c_unitPointMarker = QSharedPointer<ccSphere>(new ccSphere(1.0f, 0, "PointMarker", 6));
			
			c_unitPointMarker->showColors(true);
			c_unitPointMarker->setVisible(true);
			c_unitPointMarker->setEnabled(true);
		}

		//not sure what this does, but it looks like fun
		CC_DRAW_CONTEXT markerContext = context; //build-up point maker own 'context'
		markerContext.drawingFlags &= (~CC_DRAW_ENTITY_NAMES); //we must remove the 'push name flag' so that the sphere doesn't push its own!
		markerContext.display = 0;

		//get camera info
		ccGLCameraParameters camera;
		glFunc->glGetIntegerv(GL_VIEWPORT, camera.viewport);
		glFunc->glGetDoublev(GL_PROJECTION_MATRIX, camera.projectionMat.data());
		glFunc->glGetDoublev(GL_MODELVIEW_MATRIX, camera.modelViewMat.data());

		//set draw colour
        c_unitPointMarker->setTempColor(omega::Color::Green);
		
		//draw key-points
		const ccViewportParameters& viewportParams = context.display->getViewportParameters();
		for (unsigned i = 0; i < m_waypoints.size(); i++)
		{
			glFunc->glMatrixMode(GL_MODELVIEW);
			glFunc->glPushMatrix();

			const CCVector3* P = m_cloud->getPoint(m_waypoints[i]);
			ccGL::Translate(glFunc, P->x, P->y, P->z);
			float scale = context.labelMarkerSize * m_relMarkerScale * 0.15;
			if (viewportParams.perspectiveView && viewportParams.zFar > 0)
			{
				//in perspective view, the actual scale depends on the distance to the camera!
				const double* M = camera.modelViewMat.data();
				double d = (camera.modelViewMat * CCVector3d::fromArray(P->u)).norm();
				double unitD = viewportParams.zFar / 2; //we consider that the 'standard' scale is at half the depth
				scale = static_cast<float>(scale * sqrt(d / unitD)); //sqrt = empirical (probably because the marker size is already partly compensated by ccGLWindow::computeActualPixelSize())
			}
			glFunc->glScalef(scale, scale, scale);
			c_unitPointMarker->draw(markerContext);
			glFunc->glPopMatrix();
		}

		//set draw colour
        c_unitPointMarker->setTempColor(omega::Color::Yellow);

		//draw trace points
		for (std::deque<int> seg : m_trace)
		{
			for (int p : seg)
			{
				glFunc->glMatrixMode(GL_MODELVIEW);
				glFunc->glPushMatrix();

				const CCVector3* P = m_cloud->getPoint(p);
				ccGL::Translate(glFunc, P->x, P->y, P->z);
				float scale = context.labelMarkerSize * m_relMarkerScale * 0.1;
				if (viewportParams.perspectiveView && viewportParams.zFar > 0)
				{
					//in perspective view, the actual scale depends on the distance to the camera!
					const double* M = camera.modelViewMat.data();
					double d = (camera.modelViewMat * CCVector3d::fromArray(P->u)).norm();
					double unitD = viewportParams.zFar / 2; //we consider that the 'standard' scale is at half the depth
					scale = static_cast<float>(scale * sqrt(d / unitD)); //sqrt = empirical (probably because the marker size is already partly compensated by ccGLWindow::computeActualPixelSize())
				}
				glFunc->glScalef(scale, scale, scale);
				c_unitPointMarker->draw(markerContext);
				glFunc->glPopMatrix();
			}
		}

		//draw lines
		for (std::deque<int> seg : m_trace)
		{
			glFunc->glBegin(GL_LINE_STRIP);
			for (int p : seg)
			{
				ccGL::Vertex3v(glFunc, m_cloud->getPoint(p)->u);
			}
			glFunc->glEnd();
		}
	}
}
*/

}; // namespace gigapoint
