/*---------------------------------------------------------------------------*/
/*                                                                           */
/* FILE:    MessageReader.c                                                  */
/*                                                                           */
/* PURPOSE: This TCP client shows you how to get data from Server.           */
/*          IL n'est pas prévu pour répondre.                                                                 */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Include files                                                             */
/*---------------------------------------------------------------------------*/
#include <tcpsupp.h>
#include <ansi_c.h>
#include <utility.h>
#include <cvirte.h>     
#include <userint.h>
#include "CLIENT_MessageReader.h"

/*---------------------------------------------------------------------------*/
/* Macros                                                                    */
/*---------------------------------------------------------------------------*/
#define tcpChk(f) if ((gTCPError=(f)) < 0) {ReportTCPError(); goto Done;} else

/*---------------------------------------------------------------------------*/
/* Constants   A ADAPTER en fonction du serveur                                                              */
/*---------------------------------------------------------------------------*/
#define SERVER_NAME     "192.168.177.141"    			    // en fonction du serveur 
#define SERVER_PORT     ((unsigned int)23061)	    // en fonction du serveur

#define SIZE_DATA 101  // taille max des données reçues

/*---------------------------------------------------------------------------*/
/* Global variables                                                          */
/*---------------------------------------------------------------------------*/
static int 				gPanel = 0;			// accès au panel depuis toutes les fonctions
static int				gTCPError = 0;
static int				gConnected = 0;		// indique si un client (unique) est connecté
static unsigned int		gConversationHandle;  // "handle" (entier) du socket avec le client

/*---------------------------------------------------------------------------*/
/* Internal function prototypes                                              */
/*---------------------------------------------------------------------------*/
int CVICALLBACK TCPCallback (unsigned handle, int event, int error, 
                             void *callbackData);
static void ReportTCPError (void);

/*---------------------------------------------------------------------------*/
/* This is the application's entry-point (main)                              */
/*---------------------------------------------------------------------------*/
int main (int argc, char *argv[])
{
    if (InitCVIRTE (0, argv, 0) == 0)
        goto Done;

    /* Initialize TCP client. */	  
   // ici, le client se connecte dès son lancement (pas obligatoire)
    if (ConnectToTCPServer (&gConversationHandle, SERVER_PORT, SERVER_NAME, TCPCallback, 
							NULL, 0) < 0)
	        MessagePopup ("Error", "Could not connect to server!\n"
	            "Launch the MessageWriter program before running this program.");
    else
		gConnected = 1; 	// le client est maintenant connecté avec le serveur
	
    /* Load and initialize user interface. */
    if ((gPanel = LoadPanel (0, "CLIENT_MessageReader.uir", PANEL)) < 0)
        goto Done;
    DisplayPanel (gPanel);

    /* Run program. */
    RunUserInterface ();
    
Done:   
    /* Clean up before quit */
    if (gConnected)
        DisconnectFromTCPServer (gConversationHandle);
    if (gPanel)
        DiscardPanel (gPanel);
		SetCtrlVal (gPanel, PANEL_LED, 0);
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
/*---------------------------------------------------------------------------*/
/* Handle incoming data and dropped TCP connections.                         */
/*---------------------------------------------------------------------------*/
int CVICALLBACK TCPCallback (unsigned handle, int event, int error, 
                             void *callbackData)
{
    int 		nb_octets;
	short int 	entier;
    char    	data[SIZE_DATA];

    switch (event)
        {
        case TCP_DATAREADY:
            /* Read this data. */
            tcpChk (nb_octets = ClientTCPRead (gConversationHandle, data, SIZE_DATA, 0));
         
			/* rappel sur le choix du protocole applicatif : le 1er octet des données recues 
			       vaut 1 si un entier (court) suit, 2 pour une chaîne */

			if (nb_octets>0)
			{
            /* Read and display the data. */
	            if (data[0] ==1)  // on détermine la nature des datas (ici un entier)
	                { 
					entier = (short) (data[1]<<8 | data[2]) ; 
					SetCtrlVal (gPanel, PANEL_VAL_ENTIERE, entier);   //Display the data
					SetCtrlVal (gPanel, PANEL_LED, 1);  
	                }
				else if (data[0] ==2)
	                { 
					SetCtrlVal (gPanel, PANEL_MSG_RECU, &data[1]);  //Display the data
	                }
				else	   
	                { 
	                /* Display error */
					SetCtrlVal (gPanel, PANEL_MSG_RECU, "probleme en reception !!");
	                }
			}
	        break;
			
        case TCP_DISCONNECT:
            if (handle == gConversationHandle)
                {
                /* The server we were talking to has disconnected. */
                MessagePopup ("Message", "Server has disconnected! Choice : Client will stop.");
				gConnected = 0;
				SetCtrlVal (gPanel, PANEL_LED, 0);
                }
            break;
    }

Done:
    return 0;
}

/*---------------------------------------------------------------------------*/
/* Report TCP Errors.                                                        */
/*---------------------------------------------------------------------------*/
static void ReportTCPError (void)
{
    char    messageBuffer[1024];

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

int CVICALLBACK SendCallback (int panel, int control, int event,
		void *callbackData, int eventData1, int eventData2)
{
	short int val_entiere;
	char	  texte_a_envoyer[100]="";
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
			GetCtrlVal (gPanel, PANEL_VAL_ENTIERE_WRITE, &val_entiere);

			/* generate the data and send it */ 
			/* choix d'un protocole applicatif : le 1er octet des données à envoyer vaut
			          1 si un entier (court) suit, 2 pour une chaîne   */
		
			// envoi de l'entier (trame de 3 octets exactement)
			data[0] = 1 ;   // 1 pour un entier
			data[1] = val_entiere >> 8 ;    		// MSB de l'entier
			data[2] = (unsigned char)val_entiere;	// LSB
			tcpChk (bytesWritten = ClientTCPWrite (gConversationHandle, data, 3, 0));// envoi TCP
		}
		
		else if (control==PANEL_SEND_MSG)	// 2eme bouton SEND (chaîne à envoyer)
		{	
			/* Récupère sur l'interface la valeur à envoyer */ 
			GetCtrlVal (gPanel, PANEL_MSG_A_ENVOYER, texte_a_envoyer);
 			
			// envoi de la chaîne (trame de longueur inconnue ->  strlen);   
			data[0] = 2 ;   // 2 pour indiquer une chaîne
			strcpy (&data[1], texte_a_envoyer); // recopie (caractère NULL inclus)
			nb_octets = strlen(data)+2;   // taille utile de la chaine + 2 octets (dont NULL)
			tcpChk (bytesWritten = ClientTCPWrite (gConversationHandle, 
												   data,nb_octets, 0));    // envoi TCP
		}
		break;
	}  
Done :
	return 0;
}
