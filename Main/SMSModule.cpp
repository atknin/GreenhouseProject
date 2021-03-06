#include "SMSModule.h"
#include "ModuleController.h"
#include "PDUClasses.h"
#include "InteropStream.h"
//--------------------------------------------------------------------------------------------------------------------------------
// функция хэширования строки
//--------------------------------------------------------------------------------------------------------------------------------
#define A_PRIME 54059 /* a prime */
#define B_PRIME 76963 /* another prime */
#define C_PRIME 86969 /* yet another prime */
//--------------------------------------------------------------------------------------------------------------------------------
unsigned int hash_str(const char* s)
{
   unsigned int h = 31 /* also prime */;
   while (*s) {
     h = (h * A_PRIME) ^ (s[0] * B_PRIME);
     s++;
   }
   return h; // or return h % C;
}
//--------------------------------------------------------------------------------------------------------------------------------
bool SMSModule::IsKnownAnswer(const String& line, bool& okFound)
{
  okFound = false;
  
  if(line == F("OK"))
  {
    okFound = true;
    return true;
  }
  return ( line.indexOf(F("ERROR")) != -1 );
}
//--------------------------------------------------------------------------------------------------------------------------------
void SMSModule::Setup()
{
  Settings = MainController->GetSettings();
  
  // запускаем наш сериал
  GSM_SERIAL.begin(GSM_BAUDRATE);

 
  InitQueue(); // инициализируем очередь
   
  // настройка модуля тут
}
//--------------------------------------------------------------------------------------------------------------------------------
void SMSModule::InitQueue()
{
  while(actionsQueue.size() > 0) // чистим очередь 
    actionsQueue.pop();
 
  isModuleRegistered = false;
  waitForSMSInNextLine = false;
  WaitForSMSWelcome = false; // не ждём приглашения
  needToWaitTimer = 0; // сбрасываем таймер
   
  // настраиваем то, что мы должны сделать для начала работы
  currentAction = smaIdle; // свободны, ничего не делаем
  actionsQueue.push_back(smaWaitReg); // ждём регистрации
  actionsQueue.push_back(smaSMSSettings); // настройки вывода SMS
  actionsQueue.push_back(smaUCS2Encoding); // кодировка сообщений
  actionsQueue.push_back(smaPDUEncoding); // формат сообщений
  actionsQueue.push_back(smaAON); // включение АОН
  actionsQueue.push_back(smaDisableCellBroadcastMessages); // выключение броадкастовых SMS
  actionsQueue.push_back(smaEchoOff); // выключение эха
  actionsQueue.push_back(smaCheckReady); // проверка готовности
  
}
//--------------------------------------------------------------------------------------------------------------------------------
void SMSModule::ProcessAnswerLine(const String& line)
{
  // получаем ответ на команду, посланную модулю
  if(!line.length()) // пустая строка, нечего её разбирать
    return;

  #ifdef GSM_DEBUG_MODE
    Serial.print(F("<== Receive \"")); Serial.print(line); Serial.println(F("\" answer from modem..."));
  #endif

  // проверяем, не перезагрузился ли модем
  if(line.indexOf(F("PBREADY")) != -1 || line.indexOf(F("SMS ready")) != -1)
  {
    #ifdef GSM_DEBUG_MODE
      Serial.println(F("Modem boot found, init queue.."));
    #endif

    InitQueue(); // инициализировали очередь по новой, т.к. модем либо только загрузился, либо - перезагрузился
    needToWaitTimer = 2000; // дадим модему ещё 2 секунды на раздупливание

    return;
  }


  bool okFound = false;

  switch(currentAction)
  {
    case smaCheckReady:
    {
      // ждём ответа "+CPAS: 0" от модуля
          if(line == F("+CPAS: 0")) // получили
          {
            #ifdef GSM_DEBUG_MODE
              Serial.println(F("[OK] => Modem ready."));
           #endif
           actionsQueue.pop(); // убираем последнюю обработанную команду
           currentAction = smaIdle;
          }
          else
          {
           #ifdef GSM_DEBUG_MODE
              Serial.println(F("[ERR] => Modem NOT ready, try again later..."));
           #endif
             needToWaitTimer = 2000; // повторим через 2 секунды
          }
    }
    break;

    case smaEchoOff: // выключили эхо
    {
      if(IsKnownAnswer(line,okFound))
      {
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("[OK] => ECHO OFF processed."));
        #endif
       actionsQueue.pop(); // убираем последнюю обработанную команду     
       currentAction = smaIdle;
      }
    }
    break;

    case smaDisableCellBroadcastMessages: // запретили получение броадкастовых SMS
    {
      if(IsKnownAnswer(line,okFound))
      {
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("[OK] => Broadcast SMS disabled."));
        #endif
       actionsQueue.pop(); // убираем последнюю обработанную команду     
       currentAction = smaIdle;
      }
      
    }
    break;

    case smaAON: // включили АОН
    {
      if(IsKnownAnswer(line,okFound))
      {
        if(okFound)
        {
          #ifdef GSM_DEBUG_MODE
            Serial.println(F("[OK] => AON is ON."));
          #endif
        }
          actionsQueue.pop(); // убираем последнюю обработанную команду     
          currentAction = smaIdle;
        /*
        } // if
        else
        {
          // пробуем ещё раз
          needToWaitTimer = 1500; // через некоторое время
          currentAction = smaIdle;
        }
        */
      } // known answer
      
    }
    break;

    case smaPDUEncoding: // формат PDU
    {
      if(IsKnownAnswer(line,okFound))
      {
        if(okFound)
        {
          #ifdef GSM_DEBUG_MODE
            Serial.println(F("[OK] => PDU format is set."));
          #endif
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;
        }
        else
        {
            // пробуем ещё раз
          needToWaitTimer = 1500; // через некоторое время
          currentAction = smaIdle;
        
        }
      }
      
    }
    break;

    case smaUCS2Encoding: // кодировка UCS2
    {
      if(IsKnownAnswer(line,okFound))
      {
        if(okFound)
        {
          #ifdef GSM_DEBUG_MODE
            Serial.println(F("[OK] => UCS2 encoding is set."));
          #endif
         actionsQueue.pop(); // убираем последнюю обработанную команду     
         currentAction = smaIdle;
        }
        else
        {
            // пробуем ещё раз
          needToWaitTimer = 1500; // через некоторое время
          currentAction = smaIdle;
        
        }
      }
      
    }
    break;
    

    case smaSMSSettings: // установили режим отображения входящих SMS сразу в порт
    {
      if(IsKnownAnswer(line,okFound))
      {
        if(okFound)
        {
            #ifdef GSM_DEBUG_MODE
              Serial.println(F("[OK] => SMS settings is set."));
            #endif
           actionsQueue.pop(); // убираем последнюю обработанную команду     
           currentAction = smaIdle;
        }
        else
        {
            // пробуем ещё раз
          needToWaitTimer = 1500; // через некоторое время
          currentAction = smaIdle;           
        }
      }
      
    }
    break;

    case smaWaitReg: // пришёл ответ о регистрации
    {
      if(line.indexOf(F("+CREG: 0,1")) != -1)
      {
        // зарегистрированы в GSM-сети
           isModuleRegistered = true;
            #ifdef GSM_DEBUG_MODE
              Serial.println(F("[OK] => Modem registered in GSM!"));
            #endif
           actionsQueue.pop(); // убираем последнюю обработанную команду     
           currentAction = smaIdle;
      } // if
      else
      {
        // ещё не зарегистрированы
          isModuleRegistered = false;
          needToWaitTimer = 4567; // через некоторое время повторим команду
          currentAction = smaIdle;
      } // else
    }
    break;

    case smaHangUp: // положили трубку
    {
      if(IsKnownAnswer(line,okFound))
      {
             #ifdef GSM_DEBUG_MODE
              Serial.println(F("[OK] => Hang up DONE."));
            #endif
       
           actionsQueue.pop(); // убираем последнюю обработанную команду     
           currentAction = smaIdle;
      } 
      
    }
    break;

    case smaStartSendSMS: // начинаем посылать SMS
    {
            #ifdef GSM_DEBUG_MODE
              Serial.println(F("[OK] => Welcome received, continue sending..."));
            #endif

           actionsQueue.pop(); // убираем последнюю обработанную команду     
           currentAction = smaIdle;
           actionsQueue.push_back(smaSmsActualSend); // добавляем команду на обработку
      
    }
    break;

    case smaSmsActualSend: // отослали SMS
    {
      if(IsKnownAnswer(line,okFound))
      {
            #ifdef GSM_DEBUG_MODE
              Serial.println(F("[OK] => SMS sent."));
            #endif
      
       actionsQueue.pop(); // убираем последнюю обработанную команду     
       currentAction = smaIdle;
       actionsQueue.push_back(smaClearAllSMS); // добавляем команду на обработку
      }
    }
    break;

    case smaClearAllSMS: // очистили все SMS
    {
      if(IsKnownAnswer(line,okFound))
      {
            #ifdef GSM_DEBUG_MODE
              Serial.println(F("[OK] => saved SMS cleared."));
            #endif
      
       actionsQueue.pop(); // убираем последнюю обработанную команду     
       currentAction = smaIdle;
      }
     
    }
    break;


    case smaIdle:
    {
      if(waitForSMSInNextLine) // дождались входящего SMS
      {
        waitForSMSInNextLine = false;
        ProcessIncomingSMS(line);
      }
      
      if(line.startsWith(F("+CLIP:")))
        ProcessIncomingCall(line);
      else
      if(line.startsWith(F("+CMT:")))
        waitForSMSInNextLine = true;
       
    

    }
    break;
  } // switch  
  
}
//--------------------------------------------------------------------------------------------------------------------------------
void SMSModule::ProcessIncomingSMS(const String& line) // обрабатываем входящее SMS
{
  #ifdef GSM_DEBUG_MODE
  Serial.print(F("SMS RECEIVED: ")); Serial.println(line);
  #endif


  bool shouldSendSMS = false;

  PDUIncomingMessage message = PDU.Decode(line, Settings->GetSmsPhoneNumber());
  if(message.IsDecodingSucceed) // сообщение пришло с нужного номера
  {
  
    #ifdef GSM_DEBUG_MODE
      Serial.println(F("Phone number is OK, continue..."));
    #endif

    // ищем команды
    int16_t idx = message.Message.indexOf(SMS_OPEN_COMMAND); // открыть окна
    if(idx != -1)
    {
    #ifdef GSM_DEBUG_MODE
      Serial.println(F("WINDOWS->OPEN command found, execute it..."));
    #endif

        // открываем окна
        // сохраняем команду на выполнение тогда, когда окна будут открыты или закрыты - иначе она не отработает
        queuedWindowCommand = F("STATE|WINDOW|ALL|OPEN");
        shouldSendSMS = true;
    }
    
    idx = message.Message.indexOf(SMS_CLOSE_COMMAND); // закрыть окна
    if(idx != -1)
    {
    #ifdef GSM_DEBUG_MODE
      Serial.println(F("WINDOWS->CLOSE command found, execute it..."));
    #endif

      // закрываем окна
      // сохраняем команду на выполнение тогда, когда окна будут открыты или закрыты - иначе она не отработает
      queuedWindowCommand = F("STATE|WINDOW|ALL|CLOSE");
      shouldSendSMS = true;
    }
    
    idx = message.Message.indexOf(SMS_AUTOMODE_COMMAND); // перейти в автоматический режим работы
    if(idx != -1)
    {
    #ifdef GSM_DEBUG_MODE
      Serial.println(F("Automatic mode command found, execute it..."));
    #endif

      // переводим управление окнами в автоматический режим работы
      if(ModuleInterop.QueryCommand(ctSET, F("STATE|MODE|AUTO"),false,false))
      {
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("CTSET=STATE|MODE|AUTO command parsed, process it..."));
        #endif
    
      }

      // переводим управление поливом в автоматический режим работы
      if(ModuleInterop.QueryCommand(ctSET, F("WATER|MODE|AUTO"),false,false))
      {
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("CTSET=WATER|MODE|AUTO command parsed, process it..."));
        #endif
    
      }
     
      // переводим управление досветкой в актоматический режим работы    
      if(ModuleInterop.QueryCommand(ctSET, F("LIGHT|MODE|AUTO"),false,false))
      {
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("CTSET=LIGHT|MODE|AUTO command parsed, process it..."));
        #endif
    
      }    

      shouldSendSMS = true;
    }

    idx = message.Message.indexOf(SMS_WATER_ON_COMMAND); // включить полив
    if(idx != -1)
    {
    #ifdef GSM_DEBUG_MODE
      Serial.println(F("Water ON command found, execute it..."));
    #endif

    // включаем полив
      if(ModuleInterop.QueryCommand(ctSET, F("WATER|ON"),false,false))
      {
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("CTSET=WATER|ON command parsed, process it..."));
        #endif
    
       shouldSendSMS = true;
      }
    }

    idx = message.Message.indexOf(SMS_WATER_OFF_COMMAND); // выключить полив
    if(idx != -1)
    {
    #ifdef GSM_DEBUG_MODE
      Serial.println(F("Water OFF command found, execute it..."));
    #endif

    // выключаем полив
      if(ModuleInterop.QueryCommand(ctSET, F("WATER|OFF"),false,false))
      {
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("CTSET=WATER|OFF command parsed, process it..."));
        #endif
    
        shouldSendSMS = true;
      }

    }

           
    idx = message.Message.indexOf(SMS_STAT_COMMAND); // послать статистику
    if(idx != -1)
    {
    #ifdef GSM_DEBUG_MODE
      Serial.println(F("STAT command found, execute it..."));
    #endif

      // посылаем статистику вызвавшему номеру
      SendStatToCaller(message.SenderNumber);

      // возвращаемся, поскольку нет необходимости посылать СМС с ответом ОК - вместо этого придёт статистика
      return;
    }

    if(!shouldSendSMS)
    {
        // тут пробуем найти файл по хэшу переданной команды
        if(MainController->HasSDCard())
        {
          unsigned int hash = hash_str(message.Message.c_str());
         

          #ifdef GSM_DEBUG_MODE
            Serial.print(F("passed message = "));
            Serial.println(message.Message);
            Serial.print(F("computed hash = "));
            Serial.println(hash);
          #endif
                        
          String filePath = F("sms");
          filePath += F("/");
          filePath += hash;
          filePath += F(".sms");
    
          File smsFile = SD.open(filePath);
          if(smsFile)
          {
      
          #ifdef GSM_DEBUG_MODE
            Serial.println(F("SMS file found, continue..."));
          #endif            
            // нашли такой файл, будем читать с него данные
            String answerMessage, commandToExecute;
            char ch = 0;
    
            // в первой строке у нас лежит сообщение, которое надо послать после выполнения команды.
            while(1)
            {
              ch = (char) smsFile.read();
              if(ch == -1)
                break;
                
              if(ch == '\r')
                continue;
              else if(ch == '\n')
                break;
             else
              answerMessage += ch;
            } // while
    
            ch = 0;
    
            // во второй строке - команда
            while(1)
            {
              ch = (char) smsFile.read();
              if(ch == -1 || ch =='\r' || ch == '\n')
                break;
    
             commandToExecute += ch;    
            } // while
    
            // закрываем файл
            smsFile.close();

          #ifdef GSM_DEBUG_MODE
            Serial.print(F("command to execute = "));
            Serial.println(commandToExecute);
          #endif  
            // парсим команду
            CommandParser* cParser = MainController->GetCommandParser();
            Command cmd;
            if(cParser->ParseCommand(commandToExecute,cmd))
            {
          #ifdef GSM_DEBUG_MODE
            Serial.println(F("Command parsed, execute it..."));
          #endif                
              // команду разобрали, можно исполнять
              customSMSCommandAnswer = "";
              cmd.SetIncomingStream(this);
              MainController->ProcessModuleCommand(cmd);

              // теперь получаем ответ
              if(!answerMessage.length())
                SendSMS(customSMSCommandAnswer);
              else
                SendSMS(answerMessage);
              
            } // if
    
            return; // возвращаемся, т.к. мы сами пошлём СМС с текстом, отличным от ОК
          } // if(smsFile)
          #ifdef GSM_DEBUG_MODE
          else
          {
            Serial.println(F("SMS file NOT FOUND, skip the SMS."));
          }
          #endif            
          
        } // if(MainController->HasSDCard())
        
    } // !shouldSendSMS
    
  }
  else
  {
  #ifdef GSM_DEBUG_MODE
    Serial.println(F("Message decoding error or message received from unknown number!"));
  #endif
  }

  if(shouldSendSMS) // надо послать СМС с ответом "ОК"
    SendSMS(OK_ANSWER);


  
}
//--------------------------------------------------------------------------------------------------------------------------------
size_t SMSModule::write(uint8_t toWr)
{
 customSMSCommandAnswer += (char) toWr;
 return 1; 
}
//--------------------------------------------------------------------------------------------------------------------------------
void SMSModule::ProcessIncomingCall(const String& line) // обрабатываем входящий звонок
{
  // приходит строка вида
  // +CLIP: "79182900063",145,,,"",0
  
   // входящий звонок, проверяем, приняли ли мы конец строки?
    String ring = line.substring(8); // пропускаем команду +CLIP:, пробел и открывающую кавычку "

    int idx = ring.indexOf("\"");
    if(idx != -1)
      ring = ring.substring(0,idx);

    if(ring.length() && ring[0] != '+')
      ring = String(F("+")) + ring;
      
      #ifdef GSM_DEBUG_MODE
          Serial.print(F("RING DETECTED: ")); Serial.println(ring);
      #endif

 
  if(ring != Settings->GetSmsPhoneNumber()) // не наш номер
  {
    #ifdef GSM_DEBUG_MODE
      Serial.print(F("UNKNOWN NUMBER: ")); Serial.print(ring); Serial.println(F("!"));
    #endif

 // добавляем команду "положить трубку"
  actionsQueue.push_back(smaHangUp);
    
    return;
  }

  // отправляем статистику вызвавшему номеру
   SendStatToCaller(ring); // посылаем статистику вызвавшему
  
 // добавляем команду "положить трубку" - она выполнится первой, а потом уже уйдёт SMS
  actionsQueue.push_back(smaHangUp);
 
  
}
//--------------------------------------------------------------------------------------------------------------------------------
void SMSModule::SendCommand(const String& command, bool addNewLine)
{
  #ifdef GSM_DEBUG_MODE
    Serial.print(F("==> Send the \"")); Serial.print(command); Serial.println(F("\" command to modem..."));
  #endif

  GSM_SERIAL.write(command.c_str(),command.length());
  
  if(addNewLine)
  {
    GSM_SERIAL.write(String(NEWLINE).c_str());
  }
      
}
//--------------------------------------------------------------------------------------------------------------------------------
void SMSModule::ProcessQueue()
{
  if(currentAction != smaIdle) // чем-то заняты, не можем ничего делать
    return;

    size_t sz = actionsQueue.size();
    if(!sz) // в очереди ничего нет
      return;
      
    currentAction = actionsQueue[sz-1]; // получаем очередную команду

    // смотрим, что за команда
    switch(currentAction)
    {
      case smaCheckReady:
      {
        // надо проверить модуль на готовность
      #ifdef GSM_DEBUG_MODE
        Serial.println(F("Check for modem READY..."));
      #endif
      SendCommand(F("AT+CPAS"));
      //SendCommand(F("AT+IPR=57600"));
      }
      break;

      case smaEchoOff:
      {
        // выключаем эхо
      #ifdef GSM_DEBUG_MODE
        Serial.println(F("Disable echo..."));
      #endif
      SendCommand(F("ATE0"));
      }
      break;

      case smaDisableCellBroadcastMessages:
      {
        // выключаем эхо
      #ifdef GSM_DEBUG_MODE
        Serial.println(F("Disable cell broadcast SMS..."));
      #endif
      SendCommand(F("AT+CSCB=0"));
      }
      break;

      case smaAON:
      {
        // включаем АОН
      #ifdef GSM_DEBUG_MODE
        Serial.println(F("Turn AON ON..."));
      #endif
      SendCommand(F("AT+CLIP=1"));
      }
      break;

      case smaPDUEncoding: // устанавливаем формат сообщений
      {

      #ifdef GSM_DEBUG_MODE
        Serial.println(F("Set PDU format..."));
      #endif
      
       SendCommand(F("AT+CMGF=0"));
        
      }
      break;


      case smaUCS2Encoding: // устанавливаем кодировку сообщений
      {

      #ifdef GSM_DEBUG_MODE
        Serial.println(F("Set UCS2 format..."));
      #endif
      
       SendCommand(F("AT+CSCS=\"UCS2\""));
        
      }
      break;

      case smaSMSSettings: // устанавливаем режим отображения SMS
      {
      #ifdef GSM_DEBUG_MODE
        Serial.println(F("Set SMS output mode..."));
      #endif
      SendCommand(F("AT+CNMI=2,2"));
      
      }
      break;

      case smaWaitReg: // ждём регистрации модуля в сети
      {
     #ifdef GSM_DEBUG_MODE
        Serial.println(F("Check registration status..."));
      #endif
      SendCommand(F("AT+CREG?"));
        
      }
      break;

      case smaHangUp: // кладём трубку
      {
      #ifdef GSM_DEBUG_MODE
        Serial.println(F("Hang up..."));
      #endif
      SendCommand(F("ATH"));
       
      }
      break;

      case smaStartSendSMS: // начало отсылки SMS
      {
        #ifdef GSM_DEBUG_MODE
        Serial.println(F("Start SMS sending..."));
        #endif
        
        SendCommand(commandToSend);
        commandToSend = "";
      
       
      }
      break;

      case smaSmsActualSend: // отсылаем данные SMS
      {
      #ifdef GSM_DEBUG_MODE
        Serial.println(F("Start sending SMS data..."));
      #endif
      
        SendCommand(smsToSend,false);
        GSM_SERIAL.write(0x1A); // посылаем символ окончания посыла
        smsToSend = "";
        
        
      }
      break;

      case smaClearAllSMS: // надо очистить все SMS
      {
       #ifdef GSM_DEBUG_MODE
        Serial.println(F("SMS clearance..."));
      #endif
      SendCommand(F("AT+CMGD=1,4"));
       
      }
      break;


      case smaIdle:
      {
        // ничего не делаем
      }
      break;
      
    } // switch
}
//--------------------------------------------------------------------------------------------------------------------------------
void SMSModule::Update(uint16_t dt)
{ 
  if(needToWaitTimer > 0) // надо ждать следующей команды
  {
    needToWaitTimer -= dt;
    return;
  }

  needToWaitTimer = 0; // сбрасываем таймер ожидания
  
  ProcessQueue();
  ProcessQueuedWindowCommand(dt);

}
//--------------------------------------------------------------------------------------------------------------------------------
void SMSModule::ProcessQueuedWindowCommand(uint16_t dt)
{
    if(!queuedWindowCommand.length()) // а нет команды на управление окнами
    {
      queuedTimer = 0; // обнуляем таймер
      return;
    }

    queuedTimer += dt;
    if(queuedTimer < 3000) // не дёргаем чаще, чем раз в три секунды
      return;

    queuedTimer = 0; // обнуляем таймер ожидания

       if(ModuleInterop.QueryCommand(ctGET,F("STATE|WINDOW|ALL"),false))
      {
        #ifdef GSM_DEBUG_MODE
          Serial.println(F("CTGET=STATE|WINDOW|ALL command parsed, process it..."));
        #endif
    

        // теперь проверяем ответ. Если окна не в движении - нам вернётся OPEN или CLOSED последним параметром.
        // только в этом случае мы можем исполнять команду
        const char* strPtr = PublishSingleton.Text.c_str();
        int16_t idx = PublishSingleton.Text.lastIndexOf(PARAM_DELIMITER);
        if(idx != -1)
        {
          strPtr += idx + 1;
          
              if((strstr_P(strPtr,(const char*)STATE_OPEN) && !strstr_P(strPtr,(const char*)STATE_OPENING)) || strstr_P(strPtr,(const char*)STATE_CLOSED))
              {
                // окна не двигаются, можем отправлять команду
                 if(ModuleInterop.QueryCommand(ctSET,queuedWindowCommand,false))
                 {
           
                  // команда разобрана, можно выполнять
                    queuedWindowCommand = ""; // очищаем команду, нам она больше не нужна

                    // всё, команда выполнена, когда окна не находились в движении
                 } // if
                
              } // if(state == STATE_OPEN || state == STATE_CLOSED)
              
         } // if(idx != -1)
        
      } // if(cParser->ParseCommand(F("CTGET=STATE|WINDOW|ALL")
  
}
//--------------------------------------------------------------------------------------------------------------------------------
void SMSModule::SendStatToCaller(const String& phoneNum)
{
  #ifdef GSM_DEBUG_MODE
    Serial.println("Try to send stat SMS to " + phoneNum + "...");
  #endif

  if(phoneNum != Settings->GetSmsPhoneNumber()) // не наш номер
  {
    #ifdef GSM_DEBUG_MODE
      Serial.println("NOT RIGHT NUMBER: " + phoneNum + "!");
    #endif
    
    return;
  }

  AbstractModule* stateModule = MainController->GetModuleByID(F("STATE"));

  if(!stateModule)
  {
    #ifdef GSM_DEBUG_MODE
      Serial.println(F("Unable to find STATE module registered!"));
    #endif
    
    return;
  }


  // получаем температуры
  OneState* os1 = stateModule->State.GetState(StateTemperature,0);
  OneState* os2 = stateModule->State.GetState(StateTemperature,1);

  String sms;

   if(os1)
  {
    TemperaturePair tp = *os1;
  
    sms += T_INDOOR; // сообщение
    if(tp.Current.Value != NO_TEMPERATURE_DATA)
      sms += tp.Current;
    else
      sms += NO_DATA;
      
    sms += NEWLINE;
    
  } // if 

  if(os2)
  {
    TemperaturePair tp = *os2;
  
    sms += T_OUTDOOR;
    if(tp.Current.Value != NO_TEMPERATURE_DATA)
      sms += tp.Current;
    else
      sms += NO_DATA;
    
    sms += NEWLINE;
  } // if


  // тут получаем состояние окон
  if(ModuleInterop.QueryCommand(ctGET,F("STATE|WINDOW|0"),true))
  {

    sms += W_STATE;

    #ifdef GSM_DEBUG_MODE
      Serial.println(F("Command CTGET=STATE|WINDOW|0 parsed, execute it..."));
    #endif

    const char* strPtr = PublishSingleton.Text.c_str();
     if(strstr_P(strPtr,(const char*) STATE_OPEN))
        sms += W_OPEN;
      else
        sms += W_CLOSED;


     sms += NEWLINE;
 
    #ifdef GSM_DEBUG_MODE
      Serial.print(F("Receive answer from STATE: ")); Serial.println(PublishSingleton.Text);
    #endif
  }
    // получаем состояние полива
  if(ModuleInterop.QueryCommand(ctGET,F("WATER"),true))
  {
    sms += WTR_STATE;

    #ifdef GSM_DEBUG_MODE
      Serial.println(F("Command CTGET=WATER parsed, execute it..."));
    #endif

    const char* strPtr = PublishSingleton.Text.c_str();
    if(strstr_P(strPtr,(const char*) STATE_OFF))
      sms += WTR_OFF;
    else
      sms += WTR_ON;
          
  }

  // тут отсылаем SMS
  SendSMS(sms);

}
//--------------------------------------------------------------------------------------------------------------------------------
void SMSModule::SendSMS(const String& sms)
{
  #ifdef GSM_DEBUG_MODE
    Serial.print(F("Send SMS:  ")); Serial.println(sms);
  #endif

  if(!isModuleRegistered)
  {
    #ifdef GSM_DEBUG_MODE
      Serial.println(F("Module not registered!"));
    #endif

    return;
  }

  String num = Settings->GetSmsPhoneNumber();
  if(num.length() < 1)
  {
    #ifdef GSM_DEBUG_MODE
      Serial.println(F("No phone number saved in controller!"));
    #endif
    
    return;
  }
  
  PDUOutgoingMessage pduMessage = PDU.Encode(num,sms,true);
  commandToSend = F("AT+CMGS="); commandToSend += String(pduMessage.MessageLength);

  #ifdef GSM_DEBUG_MODE
    Serial.print(F("commandToSend = ")); Serial.println(commandToSend);
  #endif

  smsToSend = pduMessage.Message; // сохраняем SMS для отправки
  WaitForSMSWelcome = true; // выставляем флаг, что мы ждём >
  actionsQueue.push_back(smaStartSendSMS); // добавляем команду на обработку
  
}
//--------------------------------------------------------------------------------------------------------------------------------
bool  SMSModule::ExecCommand(const Command& command, bool wantAnswer)
{
  UNUSED(wantAnswer);

  size_t argsCount = command.GetArgsCount();
  
  if(command.GetType() == ctSET) 
  {
    if(!argsCount) // нет аргументов
    {
      PublishSingleton = PARAMS_MISSED;
    }
    else
    {
      String t = command.GetArg(0);
      if(t == F("ADD"))
      {
        if(argsCount < 4)
        {
          PublishSingleton = PARAMS_MISSED;
        }
        else
        {
            if(MainController->HasSDCard())
            {
              
              // добавить кастомное СМС

              // получаем закодированное в HEX сообщение
              const char* hexMessage = command.GetArg(1);
              String message;

              // переводим его в UTF-8
              while(*hexMessage)
              {
                message += (char) WorkStatus::FromHex(hexMessage);
                hexMessage += 2;
              }

              // получаем его хэш
              unsigned int hash = hash_str(message.c_str());

              #ifdef GSM_DEBUG_MODE
                Serial.print(F("passed message = "));
                Serial.println(message);
                Serial.print(F("computed hash = "));
                Serial.println(hash);
              #endif
              // создаём имя файлв
              String filePath = F("sms");
              SD.mkdir(filePath);
              filePath += F("/");
              filePath += hash;
              filePath += F(".sms");

              File smsFile = SD.open(filePath,FILE_WRITE | O_TRUNC);
              if(smsFile)
              {
                // в аргументе номер 2 у нас лежит ответ, который надо послать
                hexMessage = command.GetArg(2);
                message = "";
    
                  // переводим его в UTF-8
                  while(*hexMessage)
                  {
                    message += (char) WorkStatus::FromHex(hexMessage);
                    hexMessage += 2;
                  }

                // пишем первой строчкой ответ, который надо послать
                smsFile.print(message.c_str());
                smsFile.println("");
                
                // теперь пишем команду, которую надо выполнить
                for(uint8_t i=3;i<argsCount;i++)
                {
                  const char* arg = command.GetArg(i);
                  smsFile.print(arg);
                  if(i < (argsCount-1))
                    smsFile.write('|');
                } // for
                
                smsFile.println("");

                // закрываем файл
                smsFile.close();
              } // if(smsFile)

    
              PublishSingleton = REG_SUCC;
              PublishSingleton.Status = true;
              
            } // if(MainController->HasSDCard())
            else
              PublishSingleton = NOT_SUPPORTED;
        } // else
        
      } // ADD
      else
        PublishSingleton = UNKNOWN_COMMAND;
      
    } // else have args
  }
  else
  if(command.GetType() == ctGET) //получить статистику
  {

    if(!argsCount) // нет аргументов
    {
      PublishSingleton = PARAMS_MISSED;
    }
    else
    {
      String t = command.GetArg(0);

        if(t == STAT_COMMAND) // запросили данные статистики
        {
          SendStatToCaller(Settings->GetSmsPhoneNumber()); // посылаем статистику на указанный номер телефона
        
          PublishSingleton.Status = true;
          PublishSingleton = STAT_COMMAND; 
          PublishSingleton << PARAM_DELIMITER << REG_SUCC;
        }
        else
        {
          // неизвестная команда
          PublishSingleton = UNKNOWN_COMMAND;
        } // else
    } // else have arguments
    
  } // if
 
 // отвечаем на команду
    MainController->Publish(this,command);
    
  return PublishSingleton.Status;
}
//--------------------------------------------------------------------------------------------------------------------------------

