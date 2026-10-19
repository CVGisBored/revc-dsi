#pragma once
#include "street.h"
#include "population.h"
#include "campaign.h"
#include "vehicle_models.h"
namespace slice {
class Garage {
public:
    static void interact(Game&,Street&,Population&,Campaign&,Point anchor,const VehicleModels* models=nullptr);
    static void sync(Street&,Population&,Campaign&,Point anchor);
    static void guardDrive(Street&,const Population&,const Campaign&,Point anchor,Point before);
    static int missionModel(int value){return value==1 ? 0 : value==2 ? 3 : 2;}
    static bool overlap(Point,int,int,Point,int,int);
private:
    static Population::Traffic snapshot(const Street&);
};
}
