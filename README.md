# Progetto Sistemi Operativi — Servizio di calcolo impronte SHA-256
## Specifica progetto

L'obbiettivo era di realizzare un servizio client-server per il calcolo dell'impronta SHA-256 di file multipli, usando, a scelta dello studente:
- Inter-process communication su Linux
- Pthread e FIFO su Linux
- Inter-process communication su MentOS

dove il client inviasse al server il percorso del file e ricevesse in risposta l'impronta calcolata.
Nel dettaglio, i punti richiesti erano:
- comunicazione client-server tramite FIFO;
- thread distinti per l'elaborazione concorrente di richieste multiple;
- schedulazione delle richieste pendenti in ordine di dimensione del file;
- limite fissato al numero di thread in esecuzione;
- caching in memoria delle coppie percorso-hash già servite;
- gestione delle richieste multiple simultanee sullo stesso percorso, con elaborazione unica e attesa del risultato per le richieste ridondanti;
- thread pool per il prelievo continuo delle richieste pendenti;
- meccanismo di interrogazione della cache delle richieste già processate.

___

## Implementazione

### 1.1 Metodo di implementazione

Tra le varie modalità di realizzazione proposte ho scelto quella mediante **Pthread (*POSIX thread*) e FIFO**, in quanto ho ritenuto che in questo scenario si riuscisse ad ottenere maggiore efficienza computazionale, oltre che una maggiore semplicità implementativa grazie alle comode funzioni di libreria fornite dallo standard POSIX.

Introduciamo più dettagliatamente le caratteristiche di questo metodo di implementazione:

- **Pthread** → API dello standard POSIX per creare e gestire thread in C nativamente su sistemi Unix-like. In questo progetto essenziale per implementare la computazione parallela.
- **FIFO** → *named pipes* su sistemi Unix-like che permettono di implementare l'IPC (*Inter Process Communication*). In questo progetto essenziali per implementare la logica di scambio di informazioni tra client e server.

### 2. Specifiche implementate

Le specifiche implementate nel dettaglio sono:

- Server che riceve richieste ed invia risposte tramite FIFO.
- Client che invia richieste e riceve risposte tramite FIFO.
- Istanziazione di thread distinti per elaborare richieste multiple in modo concorrente.
- Schedulazione delle richieste pendenti in ordine di dimensione del file con logica SJF (*Shortest Job First*), ovvero minore → maggiore.
- Impostazione del limite massimo di thread in esecuzione.
- Caching in memoria delle coppie percorso-hash già calcolate per ottimizzare la velocità di risposta nel caso di file ripetuti.
- Gestione delle richieste multiple simultanee per un dato percorso processando una sola richiesta ed attendendo il risultato nelle restanti richieste.
- Esecuzione dei thread implementata tramite thread pool.

#### 2.1 Client

Il processo client ha lo scopo di interfacciarsi con l'utente finale del servizio. Svolge i compiti di:

- permettere all'utente l'inserimento dei file dei quali calcolare l'impronta, avvisandolo nel caso in cui il file non esistesse. Avrà in seguito il compito di inviare i file al processo server mediante una FIFO specifica (denominata `FIFO_REQ`),
- ricevere dal server gli hash calcolati, che prenderà da una FIFO specifica (denominata `FIFO_RES`) e stampare i risultati.

##### 2.1.1 Inserimento file

All'avvio del programma client, esso chiede di inserire i file dei quali si vuole calcolare il digest. Per ogni file inserito, si verifica l'esistenza e:

- se esiste → viene scritto in `FIFO_REQ`.
- se non esiste → viene prodotto un messaggio di errore e viene chiesto di reinserire il file.

> **Difficoltà riscontrate/Scelte implementative** <br>
> *Modalità di inserimento file* → inizialmente ho avuto dei dubbi nel cercare di capire come gestire l'inserimento dei file, in quanto non sapevo se implementarlo inserendo i file come parametri del programma client, se creare un file a parte dove inserire la lista dei file desiderati dove venissero esclusi i file non esistenti oppure, come alla fine deciso, se creare un ciclo con condizione di terminazione (nel mio caso, l'inserimento del carattere f/F).
> Ho reputato quest'ultimo metodo di implementazione migliore in quanto dà maggiore flessibilità all'utente nel caso di errori di battitura.

##### 2.1.2 Stampa degli hash

Quando vengono inseriti i percorsi dei file, avviene la computazione lato server. Man mano che i thread evaderanno le richieste, scriveranno su `FIFO_RES`. Lato client, ogni qualvolta venga scritto un nuovo digest sulla FIFO, viene stampato. Dopo che l'ultimo thread ha terminato la sua esecuzione, viene chiuso il lato di scrittura di `FIFO_RES`, producendo in questo modo un EOF (*End Of File*), ovvero il segnale per il client che la computazione delle richieste è terminata. Ciò permetterà di uscire dal ciclo di lettura delle risposte del server e terminare l'esecuzione.

> **Difficoltà riscontrate/Scelte implementative** <br>
> *Evitare che vengano prodotti EOF indesiderati* → il metodo di stampa su STDOUT del contenuto puntato dal file descriptor di una FIFO aperta è abbastanza standard: un ciclo while sul metodo POSIX `read()` che termini non appena tale funzione restituisca 0, ovvero un EOF (*End Of File*).
> Ho riscontrato il problema che, impostando il numero massimo di thread = 1, il thread, computando più lentamente delle iterazioni del ciclo while, non riusciva ad aggiungere abbastanza velocemente i file sulla FIFO. Perciò, dopo che il ciclo while aveva stampato una (o poco più) volta, la FIFO rimaneva vuota e veniva prodotto un EOF indesiderato che comportava precocemente l'uscita dal ciclo e, di conseguenza, la terminazione del programma. Per risolvere questo problema non sono state apportate modifiche lato client, ma sono avvenute solo lato server (vedi qui).

#### 2.2 Server

Il processo server è il primo che deve essere avviato per il corretto funzionamento del servizio. Svolge le funzioni di:

- permettere la variazione del numero massimo di thread che nel server lavoreranno contemporaneamente per produrre i digest, modificando una costante chiamata `MAX_THREADS`,
- ricevere i file che vengono passati dal client ed inserirli all'interno di una coda ordinata (chiamata `queue`) in base alla dimensione dei file con logica SJF,
- istanziare i thread che lavoreranno nella thread pool per processare le richieste,
- eseguire i thread in maniera thread-safe, rispettando la specifica di attendere il risultato nel caso in cui, a parità di oggetto della richiesta da parte di thread concorrenti, debba essere atteso il risultato del calcolo del digest del thread che per primo ha preso in carico quel percorso.

Infine, ogni thread deve terminare l'evasione della richiesta scrivendo l'output su `FIFO_RES`.

##### 2.2.1 Impostazione limite thread

Può essere modificato il numero di thread in che effettueranno la computazione degli hash in maniera concorrente modificando una costante denominata `MAX_THREADS`. Di default questo limite è impostato a 3.

**Difficoltà riscontrate/Scelte implementative**

*Modalità di inserimento limite thread* → ho avuto dei dubbi sul significato della specifica richiesta, in quanto non mi era chiaro se l'inserimento dovesse avvenire come parametro del processo server, a runtime, oppure se bastasse impostare un limite tramite costante. Inizialmente ero convinto che la specifica richiedesse la prima o la seconda opzione (es. `./server maxThreadsVal`) oppure implementare una FIFO che, dopo che l'utente avesse inserito tale valore lato client, passasse il valore lato server per definire una lista dinamica (non si possono definire array tramite variabili) che permettesse, iterandola, di richiamare il `pthread_create()` il numero indicato di volte.

Tuttavia, rileggendo attentamente, ho capito che la specifica richiedeva solamente che si potesse inserire un valore fissato, ad esempio tramite costante, come alla fine ho fatto.

##### 2.2.2 Lettura ed ordinamento dei file

Il server legge i file man mano che vengono inseriti nella `FIFO_REQ` dal client e ne ricava tutte le informazioni grazie alla struttura e metodo POSIX `stat`. Dopo aver ottenuto le informazioni sul file, richiama il metodo `enqueue_sorted()`, il quale prende come parametri il file e la sua dimensione, li trasforma in un oggetto di tipo `FileNode` (tipo definito da una struct contenente le informazioni che servono per ordinare i file, ovvero nome file e dimensione) e lo inserisce in una coda ordinata in base alla dimensione del file, denominata `queue`.

> **Difficoltà riscontrate/Scelte implementative** <br>
> *Come salvare le informazioni dei file e ordinarli* → ho speso del tempo cercando di capire quale potesse essere il miglior approccio per salvare le informazioni dei file su una struttura di memoria dinamica (bisogna poter aggiungere nuovi file prima e man mano che vengono processati poterli togliere).
> Infine la scelta è ricaduta sull'implementazione della struct `FileNode` che contenga: il percorso del file passato come parametro, la relativa dimensione e l'elemento `FileNode` successore. In questo modo l'allocazione di memoria risulta più diretto in quanto per creare un nuovo nodo basta allocare `sizeof(FileNode)` memoria. Inoltre, sono stati creati metodi appositi che ne permettano l'inserimento ordinato (`enqueue_sorted()`), l'estrazione dell'elemento in testa (`dequeue()`) e la stampa (`print_queue()`).

##### 2.2.3 Istanziazione dei thread worker

Prima di istanziare i thread, è necessaria l'operazione di apertura della FIFO `FIFO_RES` di risposta verso il client. Sarà su di essa che i thread worker andranno a scrivere i risultati della loro esecuzione. `FIFO_RES` è unica per tutti i thread e quindi condivisa, per questo motivo dovremo passare come parametro ai thread worker lo stesso file descriptor, in modo tale che tutti i thread possano scrivere sulla stessa FIFO.

Si creano quindi i thread tramite `pthread_create()`, passando il file descriptor di `FIFO_RES` come parametro, e poi si permette la sincronizzazione tra i thread e l'attesa della terminazione di tutti quanti da parte del processo server tramite `pthread_join()`.

> **Difficoltà riscontrate/Scelte implementative** <br>
> *Gestione del parametro file descriptor dei thread* → di norma, per passare parametri alla funzione che eseguiranno i POSIX threads, sarebbe necessario creare una struct di tipo `void*` contenente gli attributi desiderati. In questo specifico caso, avevamo un solo parametro da passare alla funzione `worker()` eseguita dai threads (ovvero il file descriptor), perciò nel contesto ritenevo avesse più senso un'alternativa "più diretta" del metodo standard, che sarebbe invece più indicato per molteplici parametri.
> Dopo aver fatto qualche ricerca, il metodo che mi è sembrato più indicato è stato quello di effettuare direttamente il cast del file descriptor (denominato `fdw`). Nella fase di creazione dei thread viene effettuato un cast a `(void*)(intptr_t)fdw`, per poi andare ad effettuare un "cast-back" nel metodo `worker()` ad `(int)(intptr_t)fdw`. Grazie al tipo `intptr_t` della libreria `stdint`, si evitano tutti i problemi legati al casting di parametri da tipo `int` a puntatori.

##### 2.2.4 Esecuzione dei thread worker

L'esecuzione dei thread worker rappresenta la parte centrale del funzionamento del servizio. Quando un thread viene eseguito (oppure reitererà sulla funzione che deve eseguire dato che è una pool), recupera il `FileNode` in testa a `queue` (ovvero il file, al momento in cui lo recupera, più piccolo presente sulla coda ordinata) tramite il metodo `dequeue()`. Questa è un'operazione critica, in quanto riguarda l'accesso ad una risorsa condivisa da parte di più thread, è di centrale importanza quindi andare ad applicare la mutua esclusione tramite i metodi POSIX `pthread_mutex_lock()` e `pthread_mutex_unlock()`.

Prima di andare a calcolare il digest SHA-256 del file appena recuperato, il worker controlla che non sia già stato calcolato in precedenza e quindi che non sia già presente nella struttura di memoria denominata `cache`. `cache` altro non è che una lista concatenata di oggetti di tipo `CacheEntry`, ovvero una struct che contiene: il file, l'hash e l'elemento `CacheEntry` successivo, oltre che un indicatore di stato (una variabile intera che indica se l'hash sia già stato calcolato - 1, oppure se sia in fase di calcolo - 0), denominata `ready`, necessaria per implementare la specifica di attesa di richieste in fase di calcolo e la relativa condition variabile (variabile di tipo `pthread_cond_t`, che serve a sospendere un thread fino a che non si verifica una certa condizione, in questo caso `ready = 1`).

Tramite il metodo `cache_get()`, che prende come parametro il percorso, si controlla se l'hash relativo sia presente in cache. Se:

- è presente → si controlla se:
  - già stato calcolato (`ready = 1`) → si copia l'hash e si procede
  - non ancora calcolato (`ready = 0`) → il thread viene messo in uno stato di waiting, dal quale uscirà al verificarsi di una condizione specifica (`ready = 1`). Questa casistica si chiama attesa condizionale e si riesce ad implementare nello standard POSIX grazie al metodo `pthread_cond_wait()`.
- non è presente → innanzitutto si inserisce il percorso all'interno di `cache` con l'attributo `ready = 0`, in modo tale che eventuali thread concorrenti che debbano computare lo stesso percorso possano vedere che il file è in fase di calcolo. Dopodichè, tramite il metodo `digest_file()` si calcola l'hash, lo si inserisce all'interno di `cache` tramite il metodo `cache_put()` e si procede. L'operazione di inserimento su `cache` è critica, per questo motivo è necessario implementare la mutua esclusione.

Si scrive infine l'hash calcolato sulla FIFO, utilizzando il metodo `dprintf()` per formattare l'output nella maniera desiderata (in questo caso: "file -> hash" nel caso in cui l'hash sia stato calcolato, "file -> hash (cache)" nel caso in cui sia stato recuperato dalla cache).

L'operazione di scrittura su FIFO in questo caso è critica in quanto è una risorsa condivisa. Anche in questo caso è necessario quindi implementare la mutua esclusione.

##### 2.2.5 Print di debug

Sono stati inseriti dei print di debug lato server per mostrare in maniera semplice l'evoluzione dell'esecuzione tra:

- prima di eseguire la computazione → dove si può notare:
  - la `queue` con i percorsi in ordine di processazione,
  - la `cache` ancora vuota.
- dopo aver eseguito la computazione → dove si può notare:
  - la `queue` svuotata,
  - la `cache` riempita di tutti i file processati,
  - la `cache` svuotata dal metodo `cache_free()`.

#### 2.3 Queue

`queue` abbiamo detto essere una coda ordinata, utilizzata per memorizzare i file che devono essere processati. Vediamo ora nel dettaglio com'è implementata la sua struttura e i metodi ad essa associati.

##### 2.3.1 La struttura di queue - struct FileNode

Gli elementi di `queue` sono di tipo `FileNode`, una struttura contenente i seguenti attributi:

- il percorso del file (`char *path`),
- la dimensione del file in byte (`off_t size`) → `off_t` è un tipo dello standard POSIX specifico per le dimensioni dei file,
- il puntatore al nodo successivo (`struct FileNode *next`).

##### 2.3.2 Funzione enqueue_sorted()

La funzione `enqueue_sorted()` inserisce un nuovo file nella coda mantenendo l'ordinamento crescente rispetto alla dimensione del file con logica SJF.

Il funzionamento è il seguente:

1. Viene allocata dinamicamente memoria per un nuovo nodo.
2. Il percorso viene duplicato con `strdup()`.
3. Se la coda è vuota oppure il nuovo file ha dimensione minore rispetto al primo elemento, il nodo viene inserito in testa.
4. In caso contrario, la funzione scorre la lista fino a trovare la posizione corretta (il primo nodo con dimensione maggiore) e inserisce il nuovo elemento in quella posizione.

##### 2.3.3 Funzione dequeue()

La funzione `dequeue()` rimuove e restituisce l'elemento in testa alla coda.

Il funzionamento è il seguente:

- Se la coda è vuota, restituisce `NULL`.
- Altrimenti salva il puntatore al primo nodo, aggiorna la testa al nodo successivo e restituisce il nodo rimosso.

**NOTA**: la funzione non libera la memoria del nodo, la deallocazione dovrà essere gestita dal chiamante dopo l'utilizzo.

##### 2.3.4 Funzione print_queue()

La funzione `print_queue()` stampa su STDOUT del server il contenuto della coda, mostrando l'elenco numerato dei file presenti, in ordine di processazione. La stampa avviene scorrendo iterativamente la lista dalla testa fino all'ultimo nodo.

#### 2.4 Cache

`cache` abbiamo detto essere una lista concatenata utilizzata come cache per memorizzare le coppie percorso–hash già calcolate. Lo scopo della cache è evitare di ricalcolare l'impronta SHA-256 di un file che è già stato elaborato, migliorando così l'efficienza complessiva del servizio. Vediamo ora nel dettaglio com'è implementata la sua struttura e i metodi ad essa associati.

##### 2.4.1 La struttura di cache - struct CacheEntry

Gli elementi di `cache` sono di tipo `CacheEntry`, una struttura contenente i seguenti attributi:

- il percorso del file (`char *path`),
- l'hash SHA-256 calcolato (`char hash[65]`) → il suo valore di default è il terminatore di stringa ('\0'),
- un flag di stato (`int ready`) che indica se l'hash è stato effettivamente computato → `ready` assume valore 1 nel caso in cui l'hash sia già stato calcolato, 0 nel caso in cui sia in fase di calcolo,
- una condition variable (`pthread_cond_t cond`) per la sincronizzazione tra thread,
- il puntatore al nodo successivo (`struct CacheEntry *next`).

##### 2.4.2 Funzione cache_get()

La funzione `cache_get()` ricerca nella cache un file dato il suo percorso. I passaggi che esegue sono i seguenti:

1. Scorre sequenzialmente la lista.
2. Confronta il campo `path` di ogni nodo con il percorso richiesto mediante `strcmp()`.
3. Se trova una corrispondenza, restituisce il puntatore alla struttura `CacheEntry`.
4. Se il file non è presente, restituisce `NULL`.

Restituire l'intera struttura (e non solo l'hash) è necessario per consentire ai thread di verificare lo stato del campo `ready` ed eventualmente attendere sulla variabile condizionale associata.

##### 2.4.3 Funzione cache_put()

La funzione `cache_put()` inserisce un nuovo elemento nella cache. Funziona nel modo seguente:

1. Viene allocata dinamicamente memoria per un nuovo elemento `CacheEntry`.
2. Il percorso viene duplicato con `strdup()`.
3. Il campo `hash` viene inizializzato a stringa vuota.
4. Il flag `ready` viene inizializzato a 0 (hash non ancora disponibile).
5. Viene inizializzata la variabile condizionale tramite `pthread_cond_init()`.
6. Il nuovo nodo viene inserito in testa alla lista → scelta data dal fatto che l'accesso a `cache` viene gestito tramite un puntatore in testa alla lista, in questo modo l'inserimento è un'operazione di complessità O(1).

La funzione restituisce il puntatore alla nuova testa, permettendo al thread che calcola l'hash di aggiornarne successivamente i campi e notificare eventuali thread in attesa.

##### 2.4.4 Funzione cache_free()

La funzione `cache_free()` libera tutta la memoria allocata per la cache. Il suo funzionamento è il seguente:

1. Scorre l'intera lista.
2. Libera la stringa indicata dal puntatore `path` → è necessario richiamare la `free()` separatamente sui puntatori in quanto, deallocando esclusivamente l'elemento al quale appartengono, si andrebbe a liberare solamente il puntatore e non la stringa puntata.
3. Libera la struttura corrente.
4. Imposta infine la testa a `NULL`.

Questo metodo è necessario per garantire una corretta gestione della memoria al termine dell'esecuzione del programma.

##### 2.4.5 Funzione print_cache()

La funzione `print_cache()` stampa a video l'elenco dei file attualmente presenti nella cache.

#### 2.5 Calcolo digest

##### 2.5.1 Funzione digest_file()

La funzione `digest_file()` è basata sull'esempio fornitoci come aiuto in supporto allo sviluppo del progetto di funzione che utilizzasse la libreria OpenSSL per il calcolo dell'hash tramite algoritmo SHA-256. Tale funzione ha come parametri:

- `const char *filename` → puntatore al nome del file.
- `char output[65]` → buffer di output dove verrà inserita l'impronta in formato esadecimale (64 caratteri più terminatore)

Tale funzione esegue i seguenti passaggi:

1. apre il file in modalità sola lettura, gestendo eventuali errori di apertura del file,
2. lo legge progressivamente a blocchi,
3. aggiorna il digest in modo incrementale,
4. converte infine il risultato binario in una stringa esadecimale.
