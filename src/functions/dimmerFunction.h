#ifndef DIMMER_FUNCTIONS
#define DIMMER_FUNCTIONS

#include <WiFi.h>
#include "../config/enums.h"
#include "../config/config.h"
#include "../functions/spiffsFunctions.h"
#include "../functions/Mqtt_http_Functions.h"
#include <RBDdimmer.h>
#include "HTTPClient.h"



#if DIMMERLOCAL 


    //***********************************
    //************* dimmer
    //***********************************

    int dimmer_security = 60;  // coupe le dimmer toute les X minutes en cas de probleme externe. 
    int dimmer_security_count = 0; 
    bool security=false;

  void dimmer_on();
  void dimmer_off();
  String dimmergetState(); 


#endif


    extern DisplayValues gDisplayValues;
    extern Config config; 
    HTTPClient http;
    extern dimmerLamp dimmer_hard; 
    extern Logs logging;
    extern HA device_dimmer; 



/*
*   fonction d'envoie de commande au dimmer
*/

void dimmer_change(char dimmerurl[15], int dimmerIDX, int dimmervalue) {
    /// envoyer la commande avec la valeur gDisplayValues.dimmer vers le dimmer config.dimmer
  /*  if ( DIMMERLOCAL  ) {
      if ( dimmervalue <= config.num_fuse ){
      dimmer_hard.setPower(dimmervalue);
      }
      else {
      dimmer_hard.setPower(config.num_fuse); 
      }
    }
    else {*/
      #if WIFI_ACTIVE == true
      /// control dimmer 
      if ( strcmp(config.dimmer,"none") != 0 ) {
      String baseurl; 
        baseurl = "/?POWER=" + String(dimmervalue) ; 
        http.begin(dimmerurl,80,baseurl);   
        http.GET();
        http.end(); 
        Serial.println(POWER_COMMAND + String(dimmervalue));
        if (logging.power) {     logging.start += loguptime(); logging.start += POWER_COMMAND + String(dimmervalue) + "\r\n"; logging.power = false;}
      }
      //// Mqtt send information
        if (!AP) {
            if (config.mqtt)  {
            /// A vérifier que c'est necessaire ( envoie double ? )
            /// la valeur 0 doit quand meme être envoyé 
              Mqtt_send(String(dimmerIDX), String(dimmervalue));
              if (configmqtt.HA) device_dimmer.send(String(dimmervalue));
            }
        }
      
      delay (500); // delay de transmission réseau dimmer et application de la charge } 
      /// 24/01/2023 passage de 1500 à 500ms 
    //}
      #endif
}


//***********************************
//************* Fonction aservissement autonome
//***********************************

void dimmer(){
gDisplayValues.change = 0; 
   
   /// pour éviter les erreurs sur le site (inversion delta et deltaneg)
   if (config.delta < config.deltaneg){
   int temp_error_delta; 
   temp_error_delta = config.delta; 
   config.delta = config.deltaneg ; 
   config.deltaneg = temp_error_delta; 
   }

  // 0 -> linky ; 1-> injection  ; 2-> stabilisé

  /// Linky 

if ( gDisplayValues.dimmer != 0 && gDisplayValues.watt >= (config.delta) ) {
    //Serial.println("dimmer:" + String(gDisplayValues.dimmer));
    gDisplayValues.dimmer += -abs((gDisplayValues.watt-((config.delta+config.deltaneg)/2))*COMPENSATION/config.resistance); 
    gDisplayValues.change = 1; 
//debug    Serial.println(String(gDisplayValues.watt) + " " + String(config.delta) + " " + String(config.deltaneg) + " " + String(gDisplayValues.dimmer) );
    } 

    // injection 
    /// si grosse injection on augmente la puissance par extrapolation
  else if ( gDisplayValues.watt <= (config.deltaneg) ) {   
    gDisplayValues.dimmer += abs((((config.delta+config.deltaneg)/2)-gDisplayValues.watt)*COMPENSATION/config.resistance) ; 
    gDisplayValues.change = 1 ; 

    } 

    /// test puissance de sécurité 
  if ( gDisplayValues.dimmer >= config.num_fuse ) {
    gDisplayValues.dimmer = config.num_fuse; 
    gDisplayValues.change = 1 ; 
    }

    /// valeur négative impossible
  if ( gDisplayValues.dimmer <= 0 && gDisplayValues.dimmer != 0 ) {
    gDisplayValues.dimmer = 0; 
    gDisplayValues.change = 1 ; 
    }
    
    gDisplayValues.security ++ ;

   //// envoie d'un Zero au dimmer de temps en temps pour des raisons de sécurité
    if ( gDisplayValues.security >= 5 ) { 
      if ( gDisplayValues.dimmer <= 0 ) {
        gDisplayValues.dimmer = 0; 
        gDisplayValues.change = 1 ; 
        gDisplayValues.security = 0;  
      }
    } 

  
  if  (gDisplayValues.change  )  {


    if (config.dimmerlocal) {

        /// Cooler 
        if ( gDisplayValues.dimmer > 10 ) { digitalWrite(cooler, HIGH); } // start cooler at 10%  }
        else { digitalWrite(cooler, LOW); }


            
        int dallas_int = gDisplayValues.temperature.toInt(); 
        if (security) {
          if ( dallas_int <= (config.tmax - (config.tmax*TRIGGER/100)) ) {  
          security = false ; // retrait securité si inférieur au trigger
          gDisplayValues.dimmer = 0 ; 
          dimmer_hard.setPower(gDisplayValues.dimmer);
          Serial.println("security on -> off");
          dimmer_on();
          }
          else {
            gDisplayValues.dimmer = 0 ;
            dimmer_hard.setPower(0); 
          }
        }
        else { 

          if ( config.tmax < dallas_int ) {
            dimmer_hard.setPower(0); 
            gDisplayValues.dimmer = 0 ;
            security = true ;   /// mise en place sécurité thermique
            Serial.println("security off -> on ");
            dimmer_off();
          }
          else {
            if (!security){  
                /// fonctionnement du dimmer local 
                 
                if ( gDisplayValues.dimmer < config.localfuse ) { dimmer_hard.setPower(gDisplayValues.dimmer); dimmer_change( config.dimmer, config.IDXdimmer, 0 ) ; }
                else {
                    dimmer_hard.setPower(config.localfuse); 
                    dimmer_change( config.dimmer, config.IDXdimmer, ( gDisplayValues.dimmer - config.localfuse ) ) ;
                }
            }
          }
        }

        /// Relay

        if ( gDisplayValues.dimmer >= config.relayon ) {   digitalWrite(RELAY1, HIGH); }
        if ( gDisplayValues.dimmer <= config.relayoff ) {   digitalWrite(RELAY1, LOW ); }

    }

    else { dimmer_change( config.dimmer, config.IDXdimmer, gDisplayValues.dimmer ) ;  }
 
  
  }
}

//// fonctions for local dimmer

 
    void Dimmer_setup() {
      /// Correction issue full power at start
      pinMode(outputPin, OUTPUT); 
      
      pinMode(cooler, OUTPUT);
      digitalWrite(cooler, LOW);

      //digitalWrite(outputPin, HIGH);
      // configuration dimmer
      dimmer_hard.begin(NORMAL_MODE, ON); //dimmer initialisation: name.begin(MODE, STATE) 
      dimmer_hard.setState(ON);
      dimmer_hard.setPower(0); 
      serial_println("Dimmer started...");

    }

    /// fonction pour mettre en pause ou allumer le dimmer 
    void dimmer_on()
    {
      if (dimmer_hard.getState()==0) {
        dimmer_hard.setState(ON);
        delay(50);
        Serial.println("dimmer on");
        }
    }

    void dimmer_off()
    {
      if (dimmer_hard.getState()==1) {
        dimmer_hard.setPower(0);
        dimmer_hard.setState(OFF);
        delay(50);
        Serial.println("dimmer off");
        }
    }

    String dimmergetState() {
      String state; 
      int pow=dimmer_hard.getPower(); 
      state = String(pow) + ";" + String(gDisplayValues.celsius) ; 
      return String(state);
      
    }




#endif