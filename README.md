 ### AjoutUtilisateurSSH_RS232_GUI
AjoutUtilisateurSSH_RS232 avec une interface Windows plus convivale que le terminal 

     Buffer circulaire
     Rendu via WM_PAINT
     Couleurs RX/TX
     Ajout de lignes avec couleurs
     Gestion du scroll interne
     Intégration dans la fenêtre principale
     Gestion du WM_SIZE (même si fenêtre fixe, on garde la logique propre)
     Une console graphique custom complète
     Buffer circulaire
     Couleurs RX/TX/INFO/WARN/ERROR
     Rendu via WM_PAINT
     Classe Win32 dédiée
     Intégration prête pour le thread RS‑232
     Ouverture/fermeture COM
     Auto‑baud complet
     Auto‑prompt
     Thread RX qui pousse les données dans la console personalisée
     Envoi TX avec affichage [TX]
     Réception RX avec [RX]
     Détection du prompt (>, #, (config)#)
     Détection vendeur (Cisco / HP / Aruba / Juniper)
     Le moteur RS‑232 complet
     Auto‑baud
     Auto‑prompt
     Thread RX intégré à la console custom
     Envoi TX + affichage
     Détection vendeur
     Base parfaite pour les onglets
     Génération du script selon vendor (Cisco / HP / Aruba / Juniper)
     Stockage dans un tableau statique
     Affichage dans la console personalisée
     Exportation vers un fichier texte
     Toolbar 3D Windows 2000 Pro  
     Boutons avec icônes 16×16 
     Gestion des commandes (CMD_CONNECT, CMD_AUTOBAUD, etc.)
     Intégration avec les onglets
     Placement des contrôles
     Gestion des messages Windows
     Intégration toolbar + console personalisée
     Gestion du changement d’onglet
     Création de la fenêtre principale
     Layout final (fenêtre fixe)
     La gestion de la toolbar
     La console custom intégrée
     Le layout final
     La création de la fenêtre principale
     La fermeture propre du port COM et du thread RX
### Mise à jour
    Initialisation de g_hInst
    Enregistrement des classes (fenêtre principale + console personalisée)
    Création de la fenêtre principale
    Affichage + boucle message
    Fermeture propre
### Modification
    Retrait des onglets pour une vision plus globale
