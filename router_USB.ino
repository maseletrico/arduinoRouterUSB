#include <AndroidAccessory.h>
//#include <Ticker.h>
#include <math.h>
#include <EEPROM.h>

//Ticker timer0;

//pino Entrada pos Chave
  const byte posChavePin = 2;
//pino Entrada Sensor Cinto de Segurança
  const byte sensorCintoPin = 3;  
//pino Entrada Sensor Luz de Marcha Ré
//  const byte luzMarchaRePin = 16;  
    
  const byte Rele = 4;
  const byte LEDsinal = 5;

AndroidAccessory acc("Injetec",
         "Router USB",
         "Injetec USB accessory Router",
         "1.0",
         "http://www.injetec.com.br",
         "0000000012345678");
void setup();
void loop();

  int i = 0,contTimeoutOn=0;
  int countPulse=0;     //Contador de pulsos instantâneo
  int pulsosContados=0; // Pulsos contados a cada 200ms
  int valorAD=0;        // Valor de AD - referência 1Volt
  int statusRele=0;     //status do Relé - 0= desatracado , 1= Atracado
  int statusSinalizacao=0;   //status Sinalizacao - 0 = deslidgada , 1 = Ligada
  int posChave=1;       //indicador de tensão pós chave - 1 com tensão, 0 máquina desligada.
  int luzMarchaRe=0;       //indicador de Luz de marcha ré acesa - .
  int sensorCinto=0;       //indicador de Sensor de Cinto 
  int  eventoComando=0;
  int contTimeoutOff=0;
  int contador1seg=0;
  int ligDesl=0;
  int ligaDesligaReleCinto = 0;
  int contEstabelecendoConn=0; //contador de tempo para socket não encontrado
  String txUSB;
  byte horimetroTx[4]; 
  byte addr=0; //endereço eeprom
  boolean debounceOff=false, debounceOn=false;
  boolean contandoOn=false, contandoOff=false;
  boolean posChaveEvent=false,flag1s=false;
  boolean SinalizaHoraEfetiva=false;
  boolean ReleCintoAlerta=false;
  boolean SinalizaGoto=false;
  
  int eventoRxHardware = 1;
  unsigned long horimetro;// 4 bytes para contador de horimetro, em segundos
  byte bufferHorimetro[4];

  String charInit="#";
  String charFinal="*";
  String charSeparator="|";

  char stringRecebidaUSB [10] ;
  char releOn[6] = "ReleOn";
  char releOff[7] = "ReleOff";
  int result=100;
  int receiveStringCount=0;

void setup()
{

   // Configuração do timer1 
  TCCR1A = 0;                        //confira timer para operação normal pinos OC1A e OC1B desconectados
  TCCR1B = 0;                        //limpa registrador
  //TCCR1B |= (1<<CS10)|(1 << CS12);   // configura prescaler para 1024: CS12 = 1 e CS10 = 1
  TCCR1B |= (0<<CS10)|(1 << CS12);   // configura prescaler para 256: CS12 = 1 e CS10 = 0
 
  TCNT1 = 0xFFC2;                    // incia timer com valor para que estouro ocorra em 1 milisegundo
                                     // 65536-(16MHz/1024/1Hz) = 49911 = 0xC2F7 para 1s
                                    //65536 -(16MHz/256/1000Hz)= 65521 = 0xFFF1
  TIMSK1 |= (1 << TOIE1);           // habilita a interrupção do TIMER1
  
  //pino Entrada pos Chave
  pinMode(posChavePin, INPUT_PULLUP);
  //attachInterrupt(digitalPinToInterrupt(interruptPin), interruptPulse, FALLING);

  //Relé
  pinMode(Rele, OUTPUT);
  digitalWrite(Rele, 1);
  statusRele=1;

  //LED Sinalizacao
  pinMode(LEDsinal, OUTPUT);
  digitalWrite(LEDsinal, 0);
  statusSinalizacao=0;

  //Sensor de temperatura
  pinMode(0, INPUT);
  valorAD = analogRead(A0);

  //pisca LED INICIAL
  digitalWrite(LED_BUILTIN, LOW);
  delay(500);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(500);
  digitalWrite(LED_BUILTIN, LOW);
  delay(500);
  digitalWrite(LED_BUILTIN, HIGH);
  
  Serial.begin(115200);
  Serial.println("\r\nStart");
  
  horimetro = EEPROM.read(addr)<<8;
  horimetro += EEPROM.read(addr+1)<<8;
  horimetro += EEPROM.read(addr+2)<<8;
  horimetro += EEPROM.read(addr+3);
  Serial.print("horimetro EEPROM ");
  Serial.println(horimetro);

  //Status do posChave
    posChave=digitalRead(posChavePin);


  limpaBufferUSB();
  
  //EEPROM.begin(512);
  delay(500);
  
  acc.begin();

  
}

void loop()
{

    valorAD = analogRead(A0);
      //Caracteriza evento posChave
      //se está contando debounce Off
      if(debounceOff){
          if(contTimeoutOff >= 5000){
            posChaveEvent=true;
            posChave=digitalRead(posChavePin);
            debounceOff=false;
            contandoOff=false;
            Serial.println("fim de contagem");
          }else{
             if(!digitalRead(posChavePin)){
              //se durante o debounce o posChve for ativado, zera o contador debounce
              contTimeoutOff=0;
            }
          }
      
      }else{
        //verifica se posChave é diferente do estado atual 
        if(posChave != digitalRead(posChavePin)){
              //testa se posChave pino é OFF e zera o contador debounce
              if(digitalRead(posChavePin)){
                  contTimeoutOff=0;
                  contandoOff=true;
                  debounceOff=true;
                  Serial.println("debounce Evento ");
              }else{
                  posChave = digitalRead(posChavePin);
                  //contTimeoutOff=0;
                  //contandoOff=false;
                  posChaveEvent=true;
                  Serial.println("Pos Chave = ON");
                  delay(500);
              }
        }
      }

      //se teve evento posChave e foi posChave desliagdo, salva horimetro
      if(posChaveEvent && posChave==1){
        posChaveEvent=false;
        salvaHorimetro(horimetro);
        
      }
  bufferHorimetro[3]=(byte) horimetro & 0xFF;
  bufferHorimetro[2]=(byte) ((horimetro >>8) & 0xFF);
  bufferHorimetro[1]=(byte) ((horimetro >>16) & 0xFF);
  bufferHorimetro[0]=(byte) ((horimetro >>24)& 0xFF);
//  Serial.print("B3:");
//  Serial.println(bufferHorimetro[3]);
//  Serial.print("B2:");
//  Serial.println(bufferHorimetro[2]);
//  Serial.print("B1:");
//  Serial.println(bufferHorimetro[1]);
//  Serial.print("B0:");
//  Serial.println(bufferHorimetro[0]);
  
  
  byte msg[11];
    
  if (acc.isConnected()) {
    //Serial.println("Accessory connected. ");
    

    //leitura da USB
    while(acc.available()>0){
      int dataRead = acc.read();
      Serial.print("MSG: ");
      Serial.println(dataRead, DEC);
      stringRecebidaUSB[receiveStringCount++]=dataRead;
    }

    String myString = (char*)stringRecebidaUSB;
    Serial.println("USB Recebida");
    Serial.println(myString);
    
    //zera contador de string recebida para formar nova array
    receiveStringCount=0;
    //Compara ReleON
    if(myString.equals("ReleOn")){
        digitalWrite(Rele, 1);
        statusRele=1; 
        Serial.println("Rele Habilitado");
        eventoComando=1;
    }
//      result =  strcmp(stringRecebidaUSB,releOn);
//      if(result==0){
//          digitalWrite(Rele, 1);
//          statusRele=1; 
//          Serial.println("Rele Habilitado");
//          eventoComando=1;
//          
//          
//      }
      //Compara ReleOff
      if(myString.equals("ReleOff")){
          Serial.println("Rele Desabilitado"); 
          digitalWrite(Rele, 0);
          statusRele=0; 
          eventoComando=1;

      }

      limpaBufferUSB();
   
      horimetroTx[3]= horimetro >> 24;
      horimetroTx[2]= horimetro >> 16;
      horimetroTx[1]= horimetro >> 8;
      horimetroTx[0]= horimetro & 0xFF;
      //acc.write("#1234|44|S*",30);
      acc.write("#");
      acc.write(pulsosContados);//0
      acc.write("|");           //1
      acc.write(valorAD);       //2
      acc.write("|");           //3
      acc.write(statusRele);    //4
      acc.write("|");           //5
      acc.write(statusSinalizacao);//6
      acc.write("|");           //7
      acc.write(posChave);      //8
      acc.write("|");           //9
      acc.write(sensorCinto);   //10
      acc.write("|");           //11
      acc.write(luzMarchaRe);   //12
      acc.write("|");           //13
      acc.write(eventoComando); //14
      acc.write("|");           //15
      //acc.write(horimetroTx,4);
      acc.write(horimetroTx[3]);//16
      acc.write(horimetroTx[2]);//17
      acc.write(horimetroTx[1]);//18
      acc.write(horimetroTx[0]);//19
      acc.write("*");
      txUSB = charInit+pulsosContados + charSeparator + valorAD
          + charSeparator + statusRele + charSeparator + statusSinalizacao 
          + charSeparator + posChave + charSeparator +  sensorCinto + charSeparator 
          + luzMarchaRe + charSeparator + eventoComando + charSeparator 
          + horimetroTx[3] + horimetroTx[2] + horimetroTx[1] + horimetroTx[0]
          + charFinal;
     Serial.println(txUSB);
                
  }else{
    acc.refresh();
    Serial.println("Estabelecendo conexao com App");
//    contEstabelecendoConn++;
//    if(posChave==0){
//        if(contEstabelecendoConn==480){
//          contEstabelecendoConn=0;
//          Serial.println("Rele Acionado TimeOut USB"); 
//          digitalWrite(Rele, 1);
//          statusRele=1;
//        }
//      }else{
//        contEstabelecendoConn=0;
//          Serial.println("TimeOut USB Reestabelecido"); 
//          digitalWrite(Rele, 0);
//          statusRele=0;
//      }
  }  

  delay(500);
}
void limpaBufferUSB(){
  //limba buffer stringRecebidaUSB
      stringRecebidaUSB[0]=NULL;
      stringRecebidaUSB[1]=NULL;
      stringRecebidaUSB[2]=NULL;
      stringRecebidaUSB[3]=NULL;
      stringRecebidaUSB[4]=NULL;
      stringRecebidaUSB[5]=NULL;
      stringRecebidaUSB[6]=NULL;
      stringRecebidaUSB[7]=NULL;
      stringRecebidaUSB[8]=NULL;
      stringRecebidaUSB[9]=NULL;
      //zera contador de fuffer string recebida USB
      receiveStringCount=0;
}
void salvaHorimetro(unsigned long horasSeg){
    horimetro = horasSeg;
    Serial.println(horimetro);
    //EEPROM.write(addr,horimetro>> 24);
    Serial.print("horimetro>> 24 ");
    Serial.println(horimetro>> 24 & 0xFF);
    EEPROM.write(addr,horimetro>> 24);
    
    Serial.print("horimetro>> 16 ");
    Serial.println(horimetro>> 16 & 0xFF);
    EEPROM.write(addr+1,horimetro>> 16);

    Serial.print("horimetro>> 8 ");
    Serial.println(horimetro>> 8 & 0xFF);
    EEPROM.write(addr+2,horimetro>> 8);
    
    EEPROM.write(addr+3,horimetro & 0xFF);
    Serial.print("horimetro 0 ");
    Serial.println(horimetro & 0xFF);
          
    //EEPROM.commit();    //Store data to EEPROM
}

ISR(TIMER1_OVF_vect)  //interrupção do TIMER1 1 Segundo
{
  TCNT1 = 0xFFC2;    // Renicia TIMER

    contador1seg++;
  if(contador1seg>=1000){
    contador1seg=0;
    flag1s=true; 
    //incrementa horimetro se posChave estiver ligado
    if(posChave==0){
      horimetro++;
    }
    //sirene intermitente hora efetifa
    if(SinalizaHoraEfetiva && posChave==0){
      if(ligDesl==0){
        ligDesl=1;
      }else{
        ligDesl=0;
      }
      digitalWrite(LEDsinal, ligDesl);
    }else{
      digitalWrite(LEDsinal, 0);
    }

    //sirene intermitente cinto
    if(ReleCintoAlerta && posChave==0){
      if(ligaDesligaReleCinto==0){
        ligaDesligaReleCinto=1;
      }else{
        ligaDesligaReleCinto=0;
      }
      digitalWrite(Rele, ligaDesligaReleCinto);
    }else{
      //digitalWrite(Rele, 0);
    }
    
    //sirene on indicando Goto
    if(SinalizaGoto){
      digitalWrite(LEDsinal, 1);
    }else{
      if(!SinalizaHoraEfetiva){
        digitalWrite(LEDsinal, 0);
      }
    }

        
  }

  if(contandoOn){ 
    contTimeoutOn++;
    if(contTimeoutOn==65000){
      //recomeça a contagem
      contTimeoutOn=0;
      
    }
  }  

  if(contandoOff){
    contTimeoutOff++;
    if(contTimeoutOff==65000){
      //recomeça a contagem
      contTimeoutOff=0;
      
    }
  }
  
}
