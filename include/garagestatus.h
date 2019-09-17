// #include <structs.h>

class GarageStatus {
public:
   GarageStatus();
   bool feed(sensordata sd);  
   int getDistance();
   bool isOpen();
   bool hasCar();
   Status getStatus();
   
   int distanceDoorOpen = 20;
   int distanceDoorClosed = 150;
private:
    sensordata data;
    float variation = 0.20;
    int variationAmount = 0;
    unsigned long minStableTime = 2000;

    int stableDistance = 0;
    int currentDistance = 0;
    long feedTime = 0;



};

GarageStatus::GarageStatus(){}

bool GarageStatus::feed(sensordata sd){
    data = sd;
    if(data.distance<=4){
        return false;
    }
    if(feedTime==0) {
        feedTime = data.lastUpdate;
        currentDistance = data.distance;
    } else {
        //check if distance is withoin variation limits
        variationAmount = currentDistance*variation;
        if( (data.distance<=(data.distance+variationAmount)) && (data.distance>=(data.distance-variationAmount)) ) {
          
            currentDistance = (currentDistance + data.distance)/2;
            if(millis() - feedTime >= minStableTime){
                feedTime = 0;
                stableDistance = currentDistance;
                return true;
            }
        } else {
            
        }
    }
    return false;

}

int GarageStatus::getDistance() {
    return stableDistance;
}

bool GarageStatus::isOpen(){
    if(stableDistance <= distanceDoorOpen) {
        return true;
    }
    return false;
}

bool GarageStatus::hasCar(){
    if(stableDistance < (distanceDoorClosed*(1-variation)) && stableDistance > (distanceDoorOpen*(1-variation+1))) {
        return true;
    }
    return false;
}

Status GarageStatus::getStatus(){
    Status ret;
    ret.open = isOpen();
    ret.car = hasCar();
    ret.distance = stableDistance;
    ret.motion = data.motion;
    ret.lastUpdate = millis();
    return ret;
}
