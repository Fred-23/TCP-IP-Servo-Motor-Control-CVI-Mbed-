/*---------------------------------------------------------------------------*/
/*                                                                           */
/* FILE:    Server_MessageWriter.c                                                  */
/*                                                                           */
/* PURPOSE: This TCP server shows you how to send data (text, numeric).							 */
/*                                                                           */
/*---------------------------------------------------------------------------*/
#define SIZE_DATA 101  // taille max des données reçues
/*---------------------------------------------------------------------------*/
/* Include files                                                             */
/*---------------------------------------------------------------------------*/
#include <rs232.h>
#include <tcpsupp.h>
#include <ansi_c.h>
#include <cvirte.h>		
#include <userint.h>
#include "commcallback.h"
#include "SERVER_MessageWriter.h"
#include <formatio.h>

/*---------------------------------------------------------------------------*/
/* Macros						                                             */
/*---------------------------------------------------------------------------*/
#define tcpChk(f) if ((gTCPError=(f)) < 0) {ReportTCPError(); goto Done;} else
static int COM_PORT_MCU  =3  ;
/*---------------------------------------------------------------------------*/
/* Constants                                              					 */
/*---------------------------------------------------------------------------*/
#define SERVER_PORT		((unsigned int)23061)		// à adapter !!

/*---------------------------------------------------------------------------*/
/* Global variables				                                             */
/*---------------------------------------------------------------------------*/
static int 				gPanel = 0;			// accès au panel depuis toutes les fonctions
static int				gTCPError = 0;
static int				gConnected = 0;		// indique si un client (unique) est connecté
static unsigned int		gConversationHandle;  // "handle" (entier) du socket avec le client
static char 			gEventChar[2];
/*---------------------------------------------------------------------------*/
/* Internal function prototypes                                              */
/*---------------------------------------------------------------------------*/
int CVICALLBACK TCPCallback (unsigned handle, int event, int error, 
							 void *callbackData);
static void ReportTCPError (void);
static void UpdateUserInterface (void);
void CVICALLBACK Event_Char_Detect_Func (int portNo,int eventMask,void *callbackData);
void CVICALLBACK ISR_Reception (int portNo,int eventMask,void *callbackData);
                             
/*---------------------------------------------------------------------------*/
/* This is the application's entry-point (main).                                    */
/*---------------------------------------------------------------------------*/
int main (int argc, char *argv[])
{
	int		serverRegistered = 0;
	
	if (InitCVIRTE (0, argv, 0) == 0)
		goto Done;
	
	/* Initialize TCP server : */
	tcpChk (RegisterTCPServer (SERVER_PORT, TCPCallback, NULL));
	serverRegistered = 1;         // le serveur est lancé
	
		/*  Open and Configure Com port */
    OpenComConfig (COM_PORT_MCU, "", 9600, 0, 8, 1, 6, 6);
	
	/*  Turn off Hardware handshaking (loopback test will not function with it on) */
	SetCTSMode (COM_PORT_MCU, LWRS_HWHANDSHAKE_OFF);
	
	
	/*  Make sure Serial buffers are empty */
    FlushInQ (COM_PORT_MCU);
    FlushOutQ (COM_PORT_MCU);
	
	// Install a callback : when the chosen event appears at the receive buffer, the callback function will be notified.  
	InstallComCallback (COM_PORT_MCU, LWRS_RECEIVE, 7, 'b', ISR_Reception, 0);
	
	// AUTRE SOLUTION: déclenchement de la callback de réception sur un caractère particulier à choisir

	//  Promt the user to enter an event character. 
    //PromptPopup ("Event Character", "Enter the Event Character (lance la callback reception)", gEventChar, 1);
    
    //Install a callback such that if the event character appears at the receive buffer, our function will be notified. 
    //InstallComCallback (COM_PORT_MCU, LWRS_RXFLAG, 0, (int)gEventChar[0] , Event_Char_Detect_Func, 0);
    
	
	/* Load and initialize user interface. */
	if ((gPanel = LoadPanel (0, "SERVER_MessageWriter.uir", PANEL)) < 0)
		goto Done;
	UpdateUserInterface ();  //inverse l'accessibilité des zones de saisie (Dimmed)
	DisplayPanel (gPanel);
	
	/* Run program. */
	RunUserInterface ();
	

	 
	
	
	

	
	
	
	
	/*  Close the open COM port before exiting */
    CloseCom (COM_PORT_MCU);
Done:	
	/* Clean up */
	if (gPanel)
		DiscardPanel (gPanel);
	if (serverRegistered)
		UnregisterTCPServer (SERVER_PORT);
	return 0;
}		
/*---------------------------------------------------------------------------
	Callback associée aux DEUX boutons Send de l'interface UIR (rare) :
	Read on the interface, generate data, and send it to connected client.            
/*---------------------------------------------------------------------------*/
int CVICALLBACK SendCallback (int panel, int control, int event,
		void *callbackData, int eventData1, int eventData2)
{
	short int val_entiere;
	char	  texte_a_envoyer[100]="Integer received";
	char	  acquittement[100]="Message received";    
	char	  data[101]; // datas de la trame TCP
	int		  bytesWritten;
	int 	  nb_octets;
	
	switch (event)
	{
	case EVENT_COMMIT:
		if (control==PANEL_SEND_ENTIER)	 //pour savoir quel bouton SEND a déclenché la callback
		 			// 1er bouton SEND (entier à envoyer) 
		{
			/* Récupère sur l'interface la valeur à envoyer */
			GetCtrlVal (gPanel, PANEL_VAL_ENTIERE, &val_entiere);

			/* generate the data and send it */ 
			/* choix d'un protocole applicatif : le 1er octet des données à envoyer vaut
			          1 si un entier (court) suit, 2 pour une chaîne   */
		
			// envoi de l'entier (trame de 3 octets exactement)
			data[0] = 1 ;   // 1 pour un entier
			data[1] = val_entiere >> 8 ;    		// MSB de l'entier
			data[2] = (unsigned char)val_entiere;	// LSB
			tcpChk (bytesWritten = ServerTCPWrite (gConversationHandle, data, 3, 0));// envoi TCP
			MessagePopup ("OK entier", texte_a_envoyer);
		}
		
		else if (control==PANEL_SEND_MSG)	// 2eme bouton SEND (chaîne à envoyer)
		{	
			/* Récupère sur l'interface la valeur à envoyer */ 
			GetCtrlVal (gPanel, PANEL_MSG_A_ENVOYER, texte_a_envoyer);
 			
			// envoi de la chaîne (trame de longueur inconnue ->  strlen);   
			data[0] = 2 ;   // 2 pour indiquer une chaîne
			strcpy (&data[1], texte_a_envoyer); // recopie (caractère NULL inclus)
			nb_octets = strlen(data)+2;   // taille utile de la chaine + 2 octets (dont NULL)
			tcpChk (bytesWritten = ServerTCPWrite (gConversationHandle, 
												   data,nb_octets, 0));    // envoi TCP
			MessagePopup ("OK entier", acquittement);
			
		}
		break;
	}  
Done :
	return 0;
}
/*---------------------------------------------------------------------------*/
// CALLBACK ASSOCIEE A LA LIAISON TCP(cad déclenchée par les 3 évènements TCP)
// Handle incoming and dropped TCP connections.                     		 
/*---------------------------------------------------------------------------*/
int CVICALLBACK TCPCallback (unsigned handle, int event, int error, 
							 void *callbackData)
{
	int 		nb_octets;
	short int 	entier;
    char    	data[SIZE_DATA]; 
	
    switch (event)
        {
        case TCP_CONNECT:
            if (gConnected)     /* We already have one client; don't accept another. */
                {
                tcpChk (DisconnectTCPClient (handle));
                }
            else  				/* Handle this new client connection */  
                { 
                gConversationHandle = handle;
				UpdateUserInterface ();  //inverse l'accessibilité des zones de saisie (Dimmed)
                
				// Set the disconnect mode, so we do not need to close the connection ourselves.
                tcpChk (SetTCPDisconnectMode (gConversationHandle,TCP_DISCONNECT_AUTO));
                
				gConnected = 1; 
				SetCtrlVal(gPanel,PANEL_LED,1); 
                }
            break;
			
        case TCP_DATAREADY:
        	tcpChk (nb_octets = ServerTCPRead (gConversationHandle, data, SIZE_DATA, 0));
         
			/* rappel sur le choix du protocole applicatif : le 1er octet des données recues 
			       vaut 1 si un entier (court) suit, 2 pour une chaîne */
		//char info[6]={"A 090"};
		
			if (nb_octets>0)
			{
            /* Read and display the data. */
	            if (data[0] ==1)  // on détermine la nature des datas (ici un entier)
	                { 
					entier = (short) (data[1]<<8 | data[2]) ; 
					SetCtrlVal (gPanel, PANEL_VAL_ENTIERE_LUE, entier);   //Display the data
	                
					}
				else if (data[0] ==2)
	                { 
					SetCtrlVal (gPanel, PANEL_MSG_CLIENT, &data[1]);  //Display the data
					/*----------------------------------------------NEW_Version_Fred_et_Roman-----------------------------------------*/
					/*FlushInQ (COM_PORT_MCU);
           		    //ComWrt (COM_PORT_MCU, info, strlen(info));
					ComWrt (COM_PORT_MCU, &data[1], 6);*/
	                }
				else	   
	                { 
	                /* Display error */
					SetCtrlVal (gPanel, PANEL_MSG_CLIENT, "probleme en reception !!");
	                }
			}
            break;
			
        case TCP_DISCONNECT:
            if (handle == gConversationHandle)
                {
                /* The client we were talking to has disconnected. */
                gConnected = 0;
				SetCtrlVal(gPanel,PANEL_LED,0);
				
                UpdateUserInterface ();
                }
            break;
    }

Done:    
    return 0;
}
/*---------------------------------------------------------------------------*/
/* Respond to the panel closure to quit the UI loop 						 */
/*  (callback associée au clic sur la croix de fermeture de la fenêtre).     */
/*---------------------------------------------------------------------------*/
int CVICALLBACK PanelCallback (int panel, int event, void *callbackData,
		int eventData1, int eventData2)
{
	switch (event)
		{
		case EVENT_CLOSE:
			QuitUserInterface (0);
			break;
		}
	return 0;
	
}

/********************************************************************************************/
/* ISR_Reception ():  Fuction called when the chosen event is detected.         
   					  Read the data on the serial COM port. 
/********************************************************************************************/
void CVICALLBACK ISR_Reception (int portNo,int eventMask,void *callbackData)
{
    char    readBuf[40] = {0};
    int     strLen;
    
    /*  Read the characters from the port */
    strLen = GetInQLen (COM_PORT_MCU);
    ComRd (COM_PORT_MCU, readBuf, 4);
	SetCtrlVal (gPanel, PANEL_MSG_microC, readBuf);
	
    //SetActiveCtrl (gPanelHandle, PANEL_INPUT_STRING);

    return;
}  

/********************************************************************************************/
/* Event_Char_Detect_Func ():  Fuction called when the event character is detected.         */
/********************************************************************************************/
void CVICALLBACK Event_Char_Detect_Func (int portNo,int eventMask,void *callbackData)
{
    char outMessage[256];
    
    Fmt (outMessage, "%s<%s%s%s",
          "The Character \"",gEventChar,"\" was detected during reception on COM_PORT");
    MessagePopup ("Install CommCallback", outMessage);

    return;
}   

/*---------------------------------------------------------------------------*/
/* Update user interface dimming 
   (l'attribut met en gris les zones non accessibles de l'IHM (DIMMED).      */ 
/*---------------------------------------------------------------------------*/
static void UpdateUserInterface (void)
{
	/*SetCtrlAttribute (gPanel, PANEL_SEND_MSG, ATTR_DIMMED, !gConnected);
	SetCtrlAttribute (gPanel, PANEL_SEND_ENTIER, ATTR_DIMMED,!gConnected);
      
	SetCtrlAttribute (gPanel, PANEL_MSG_A_ENVOYER, ATTR_DIMMED, !gConnected);
	SetCtrlAttribute (gPanel, PANEL_VAL_ENTIERE, ATTR_DIMMED, !gConnected);			*/
	
}   
/*---------------------------------------------------------------------------*/
/* Report TCP Errors.                      								 	 */
/*---------------------------------------------------------------------------*/
static void ReportTCPError (void)
{
	char	messageBuffer[1024];

	if (gTCPError < 0)
		{
		sprintf(messageBuffer, 
			"TCP library error message: %s\nSystem error message: %s", 
			GetTCPErrorString (gTCPError), GetTCPSystemErrorString());
		MessagePopup ("Error", messageBuffer);
		gTCPError = 0;
		}
}
/*---------------------------------------------------------------------------*/

	 
int CVICALLBACK CMD_UC (int panel, int control, int event,
						void *callbackData, int eventData1, int eventData2)
{   char info[20];
	char capteur[20];

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal (gPanel, PANEL_MSG_CLIENT, info);
			FlushInQ (COM_PORT_MCU);
   		    ComWrt (COM_PORT_MCU, info, strlen(info));
			
			//envoie au client
			GetCtrlVal (gPanel, PANEL_MSG_microC, capteur);
			SetCtrlVal (gPanel, PANEL_MSG_A_ENVOYER, capteur);
			break;
	}
	return 0;
}
