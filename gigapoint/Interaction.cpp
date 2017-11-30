#include "Interaction.h"
#include "PointCloud.h"
#include "FractureTracer.h"
#include "NodeGeometry.h"

using namespace std;
using namespace omicron;

namespace gigapoint {
Interaction::Interaction(PointCloud *p): m_cloud(p) ,interactMode(INTERACT_NONE),tracerRED(NULL),tracerGREEN(NULL),tracerBLUE(NULL),tracer(NULL),m_drawTrace(true)
{
    windowWidth=m_cloud->getWidth();
    windowHeight=m_cloud->getHeight();
}

Interaction::~Interaction() {
    if(tracerRED)
        delete tracerRED;
    if(tracerGREEN)
        delete tracerGREEN;
    if(tracerBLUE)
        delete tracerBLUE;
}

void Interaction::setTracerByPlayerId(int playerid) {
    switch (playerid) {
            case 1: if (tracerRED==NULL) {tracerRED=new FractureTracer(m_cloud);} ;tracer=tracerRED; break;
            case 2: if (tracerGREEN==NULL) {tracerGREEN=new FractureTracer(m_cloud);} ;tracer=tracerGREEN; break;
            case 3: if (tracerBLUE==NULL) {tracerBLUE=new FractureTracer(m_cloud);} ;tracer=tracerBLUE; break;
            default: std::cout << "unhandled playerid" << playerid << std::endl;
        }
}

void Interaction::resetTracer(int playerid)
{
    setTracerByPlayerId(playerid);
    tracer->destroy();

}

int Interaction::test(int playerID) {
    setTracerByPlayerId(playerID);
    return tracer->test(playerID);
    /*
    NodeGeometry *node;
    std::cout << "intertest" << std::endl;
    m_cloud->tryGetNode("r4",node);
    //std::cout << b << node << std::endl;
    PointIndex p1(node,7719);
    PointIndex p2(node,7719);
    // compare p1 and p2 should be different adress
    std::map< PointIndex,int > m;
    m[p1]=1;
    m.erase(p2);
    int j=7;
    */
}

//void Interaction::next() { tracer->next(); }

void Interaction::draw()
{
    // draw interaction
    if(interactMode != INTERACT_NONE) {
        glDisable(GL_LIGHTING);
        glDisable(GL_BLEND);
        //draw ray line
        Vector3f spos = ray.getOrigin(); //- 1*ray.getDirection();
        Vector3f epos = ray.getOrigin() + 100*ray.getDirection();
        glLineWidth(4.0);
        glColor3f(1.0, 1.0, 1.0);
        glBegin(GL_LINES);
        glVertex3f(spos[0], spos[1], spos[2]);
        glVertex3f(epos[0], epos[1], epos[2]);
        glEnd();

        glEnable(GL_PROGRAM_POINT_SIZE_EXT);
        glPointSize(20);
        glColor3f(0.0, 1.0, 0.0);
        glBegin(GL_POINTS);
        for(int i=0; i < hitPoints.size(); i++) {
            glVertex3f(hitPoints[i]->position[0], hitPoints[i]->position[1], hitPoints[i]->position[2]);
        }
        glEnd();
    }
    drawTrace();
}

void Interaction::drawTrace()
{
    if (!m_drawTrace)
        return;
    if (tracerRED!=NULL)
        if (tracerRED->destroyme()) {
            delete tracerRED;
            tracerRED=NULL;
        } else { tracerRED->render();}
    if (tracerGREEN!=NULL)
        if (tracerGREEN->destroyme()) {
            delete tracerGREEN;
            tracerGREEN=NULL;
        } else { tracerGREEN->render();}
    if (tracerBLUE!=NULL)
        if (tracerBLUE->destroyme()) {
            delete tracerBLUE;
            tracerBLUE=NULL;
        } else { tracerBLUE->render();}
}

void Interaction::useSelectedPointAsTracePoint()
{
    if (hitPoints.size()==0)
        return;
    cout << "Inserting new Tracepoint Node / PointIndex:" << hitPoints[0]->node->getName() << " / " << hitPoints[0]->index << std::endl;
    Point p1(hitPoints[0]->node,hitPoints[0]->index);
    p1.index.node->getPointData(p1);
    tracer->insertWaypoint(p1);
}


void Interaction::pickPointFromRay(const omega::Vector3f &origin,const omega::Vector3f &direction,int playerid) {
    setTracerByPlayerId(playerid);    
    ray.setOrigin(origin);
    ray.setDirection(direction);
    findHitPoint();
}



// interaction
void Interaction::updateRay(const omega::Ray& r) {
    ray = r;
}


void Interaction::traceAllFractures()
{
    if (tracerRED!=NULL)
        tracerRED->update();
    if (tracerGREEN!=NULL)
        tracerGREEN->update();
    if (tracerBLUE!=NULL)
        tracerBLUE->update();
    //setTracerByPlayerId(1);
    //tracer->update();
}

void Interaction::traceFracture(int playerid)
{

    setTracerByPlayerId(playerid);
    if (tracer->waypoint_count()<2)
    {
        cout << "not tracing, not enough waypoints" << endl;
        return;
    }
    tracer->setTracerStatus(FractureTracer::START);
    tracer->debugSegment();
    //bool success = true;//tracer->optimizePath(1000000);
    //if (success)
//        cout << "Tracing was sucessfull #Tracepoints:" << tracer->getTraceRef().size() << endl;
  //  else
    //    cout << "Tracing was not sucessfull" << endl;
}


void Interaction::updateInteractionMode(const string& mode)
{
    if(mode.compare("None")==0)
        interactMode=INTERACT_NONE;
    else if(mode.compare("Point")==0)
        interactMode=INTERACT_POINT;
    else if(mode.compare("MultiFracture")==0)
        interactMode=INTERACT_MULTIFRACTURE;
    else
        std::cerr << "Unable to set pointcloud::interact mode to "<< mode << " Supported modes are [None | Point | MultiFracture]" << std::endl;
}

bool hitpointcomparator (HitPoint* i,HitPoint* j) { return (i->distance<j->distance); }

void Interaction::findHitPoint() {

    if (interactMode == INTERACT_POINT) {
        unsigned int start_time = Utils::getTime();
        hitPoints.clear();
        HitPoint* point = new HitPoint();
        std::list<NodeGeometry*> displayList = m_cloud->getListOfVisibleNodesRef();
        for(list<NodeGeometry*>::iterator it = displayList.begin(); it != displayList.end(); it++) {
            NodeGeometry* node = *it;
            node->findHitPoint(ray, point);            
        }
        hitPoints.push_back(point);
        std::sort (hitPoints.begin(), hitPoints.end(), hitpointcomparator);
        if(point->distance != -1) {
            hitPoints.push_back(point);
            cout << "find time: " << Utils::getTime() - start_time << " size: " << hitPoints.size()
                << " dis: " << hitPoints[0]->distance
                << " pos: " << hitPoints[0]->position[0] << " " << hitPoints[0]->position[1] << " "
                << hitPoints[0]->position[2] << endl;
            if (hitPoints.size() >=2 ) {
                float dis=Utils::distance(hitPoints[0]->position,hitPoints[1]->position);
                cout << "distance between first and second point is: " << dis << endl;
                cout << hitPoints[0]->node->getName() << " " << hitPoints[0]->index << std::endl;
                cout << hitPoints[0]->distance << " " << hitPoints[hitPoints.size()-1]->distance << endl;
            }
        }
        else {
            cout << "MISS" << endl;
            hitPoints.clear();
        }
    }
}


} // namespace Interaction