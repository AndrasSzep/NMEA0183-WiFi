#ifndef _BoatData_H_
#define _BoatData_H_

struct  sBoatData{
  String TrueHeading,SOG = "6",COG = "13",Variation,
         GPSTime, UTC = "22:04:28", Date = "28.04.1957", // Secs since midnight,
         Latitude, Longitude, Altitude, HDOP, GeoidalSeparation, DGPSAge,
         WaterTemperature = "4" , WaterDepth = "4" , Offset,
         RPM, Humidity = "50", Pressure = "760", AirTemperature = "21",
         WindDirectionT, WindDirectionM, WindSpeedK, WindSpeedM,
         WindAngle ;
};

struct tBoatData {
  unsigned long DaysSince1970;   // Days since 1970-01-01
  
  double TrueHeading,SOG,COG,Variation,
         GPSTime,// Secs since midnight,
         Latitude, Longitude, Altitude, HDOP, GeoidalSeparation, DGPSAge,
         WaterTemperature, WaterDepth, Offset,
         RPM,
         WindDirectionT, WindDirectionM, WindSpeedK, WindSpeedM,
         WindAngle ;
  int GPSQualityIndicator, SatelliteCount, DGPSReferenceStationID;
  bool MOBActivated;
  char Status;

public:
  tBoatData() {
    TrueHeading=0;
    SOG=0;
    COG=0; 
    Variation=7.0;
    GPSTime=0;
    Latitude = 0;
    Longitude = 0;
    Altitude=0;
    HDOP=100000;
    DGPSAge=100000;
    WaterTemperature = 0;
    DaysSince1970=0; 
    MOBActivated=false; 
    SatelliteCount=0; 
    DGPSReferenceStationID=0;
  };
};

#endif // _BoatData_H_
