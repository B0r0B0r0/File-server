Tema 2:

Rulare:
	Pentru a rula aplicatia, sursa serverului si sursa clientului (impreuna cu makefile-urile) trebuie puse si compilate in foldere diferite. De asemenea, atat server-ul cat si clientul trebuie sa aibe un folder numit Resources, si un folder numit Downloads (care se va afla in folder-ul Resources, in Resources/Downloads).

Detalii tehnice:
	Clientul ramane conectat pana cand acesta isi cere singur deconectarea, prin trimiterea mesajului 40, sau raspunderea cu 'n' la 'Do you wish to continue? [y/n]:'.

	Caile trimise ca parametru de catre clienti, atat pentru operatiile efectuate pe memoria serverului cat si cele de pe memoria clientului, sunt relative fata de folder Resources. (e.g. Downloads/file1.txt).

	Server-ul lucreaza multithreaded. Acesta dispune de un thread care asculta si accepta conexiuni si care creaza threaduri separate pentru fiecare client. De asemenea, Server-ul mai detine si un thread care se ocupa cu graceful termination la semnalele SIGINT si SIGTERM, cat si la introducerea cuvantului 'quit' la standard input. Server-ul mai dispune si de un thread care actualizeaza vectorul de frecventa pentru fiecare fisier stocat.

	Atat server-ul cat si clientul comunica prin portul 2002.

	Statusurile protocoalelor folosite sunt transmise prin 4 octeti. 

	Server-ul dispune de o functionalitate de log, acesta scrie in fisierul log.txt toate comenzile ce au fost rulate cu succes.