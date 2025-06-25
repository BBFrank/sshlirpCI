# sshlirpCI

**Continuous Integration for sshlirp - Instant VPN**

## Introduzione

sshlirpCI nasce per dare vita a un sistema automatizzato di Continuous Integration per il progetto sshlirp - Instant VPN.
sshlirpCI infatti si pone l'obbiettivo di esporre binari di sshlirp sempre aggiornati all'ultima versione del codice sorgente e compilati per diverse architetture.

Il progetto si basa sul funzionamento di un demone che, a intervalli regolari, si occupa di reperire l'ultima versione disponibile di sshlirp ed effettuarne la cross-compilazione per le achitetture indicate in parallelo.

L'intero programma è progettato per loggare su file ogni singola operazione effettuata sia dal processo principale che dai thread, in modo che, in caso di warning, errori, crash o anche per puro scopo di monitoraggio dell'avanzamento, l'utente possa in qualsiasi momento conoscere lo stato del processo.

Il funzionamento del demone si basa infine su variabili modificabili a piacere dall'utente, contenute nel file di configurazione `ci.conf` e che definiscono le architetture per cui si vuole effettuare la cross-compilazione, il path della directory target che si desidera contenga gli eseguibili finali, i paths dei log files, i percorsi delle directory di "lavoro" di sshlirpCI, l'url per il clone di sshlirp e l'intervallo di tempo tra un ciclo di aggiornamento dei binari e l'altro.
Tale flessibilità in futuro potrebbe permettere, attraverso alcune e non sostanziali modifiche al codice di sshlirpCI, di ampliare tale progetto a un sistema di CI funzionante anche per altri progetti in via di sviluppo (oltre a sshlirp).

## Prerequisiti

### Qemu static user e debootstrap

`sshlirp_ci_start` (l'eseguibile di avvio del demone ottenuto tramite la compilazione di sshlirpCI - sezione [Compilazione](#compilazione)), come anticipato, si basa sull'utilizzo di qemu static user per effettuare una cross compilazione tra la macchina host e le architetture target e per fare ciò si appoggia a debootstrap. Prerequisito fondamentale è quindi installare tali pacchetti eseguendo sul proprio host i seguenti comandi:

```sh
sudo apt update
sudo apt upgrade
sudo apt install debootstrap qemu-user-static
```

Inoltre, affinchè l'esecuzione degli script embedded (generati dal demone stesso e il cui contenuto è consultabile nella cartella `script`) funzioni, è necessario che l'utente abbia configurato `apt` in modo che questo installi i pacchetti richiesti al path `/usr/bin/`.

### Dipendenze

sshlirpCI si basa sull'utilizzo di librerie standard di C e non utilizza librerie aggiuntive.
Tuttavia in ambienti minimali, per quanto improbabile, potrebbe essere necessario eseguire il comando di installazione del pacchetto `build-essential`.
Inoltre per un corretto clone dal repo di sshlirpCI e una fase di build funzionante sarà necessario installare i pacchetti `git` e `cmake`.

```sh
sudo apt install build-essential cmake git
```

### Permessi

Per poter eseguire correttamente sshlirpCI è necessario che l'utente sia in possesso dei privilegi root e quindi che abbia accesso al comando sudo, oltre che al file `/etc/sudoers` (che sarà necessario modificare per garantire i privilegi root anche ai thread di compilazione).
In alternativa è sempre possibile esguire sshlirpCI su una macchina virtuale, dove è certo che l'utente possa soddisfare questo requisito.

## Clone

Attualmente sshlirpCI è disponibile su un repository di GitHub e clonabile attraverso il seguente comando:

```sh
git clone https://github.com/BBFrank/sshlirpCI.git
```

Una volta clonato il repository sarà necessario modificare l'unico path hardcodato presente nel codice, ossia il path che indica al programma la posizione di `ci.conf`. La modifica è da effettuare nel file `src/include/types/types.h`, sostituendo la riga n.6 con:

```c
#define DEFAULT_CONFIG_PATH "/path/to/sshlirpCI/ci.conf"
```
    
(dove `/path/to/sshlirpCI` è il percorso in cui è stato clonato il repository di sshlirpCI)

## Compilazione

Per compilare sshlirpCI è necessario seguire i seguenti passaggi:

```sh
cd sshlirpCI
mkdir build
cd build
cmake ..
make
```

Questa serie di comandi creerà una directory `build` all'interno della directory di sshlirpCI, in cui saranno presenti i seguenti eseguibili:

- `sshlirp_ci_start`: l'eseguibile che avvia il demone di sshlirpCI
- `sshlirp_ci_stop`: l'eseguibile che ferma il demone di sshlirpCI e pulisce i file temporanei

## Modifica dei permessi

Come conseguenza diretta dell'uso di debootstrap (che non può eseguire se non come root) da parte dell'eseguibile di `sshlirp_ci_start` e a causa dello svolgimento di operazioni che richiedono privilegi root da parte sia di `sshlirp_ci_start` che di `sshlirp_ci_stop`, prima di poter lanciare l'eseguibile di start, è necessario modificare il file `/etc/sudoers` in modo che i due eseguibili, se lanciati con il comando `sudo`, possano avere accesso ai privilegi root. Per far ciò è sufficiente aggiungere le seguenti righe al file citato sopra:

```
user ALL=(root) NOPASSWD: /path/to/sshlirpCI/build/build/sshlirp_ci_start
user ALL=(root) NOPASSWD: /path/to/sshlirpCI/build/build/sshlirp_ci_stop
```

## Avvio del demone

Per avviare il demone è sufficiente lanciare il seguente comando, sostituendo `/path/to/sshlirpCI` con il percorso in cui è stato clonato il repository di sshlirpCI:

```sh
sudo /path/to/sshlirpCI/build/build/sshlirp_ci_start
```

## Monitoring del demone - i log files

Durante l'esecuzione del demone (ossia quando questo non è in stato di sleep, in attesa di un aggiornamento del codice sorgente di sshlirp), il main process e i thread, da esso lanciati per ogni architettura target definita in `ci.conf`, loggano ogni operazione su file separati, di cui verrà fatto il merge dal main, solo in fase conclusiva.
Perciò, sebbene l'utente possa limitarsi a osservare il file di log principale (il cui percorso è salvato nella variabile `LOG_FILE` in `ci.conf`) a fine esecuzione per riscontare eventuali errori, potrebbe essere sua intenzione monitorare l'avanzamento del processo in tempo reale.
Per far ciò è sempre possibile, durante l'esecuzione del demone, consultare i file di log dei singoli thread:

- **log file del thread nell'host**: questo è reperibile nella directory `THREAD_LOG_DIR` (variabile salvata nel file di configurazione)
- **log file del thread nel chroot associato**: questo è reperibile nella directory `MAIN_DIR/${arch}-chroot/THREAD_CHROOT_LOG_FILE`

È importante specificare che prima che il demone entri in stato di `SLEEPING`, tutti i file di log vengono mergeati in `LOG_FILE` e poi svuotati del loro contenuto.
Lo stato e il pid del processo, quando attivo, sono sempre consultabili nei file `/tmp/sshlirp_ci.state` e `/tmp/sshlirp_ci.pid` rispettivamente.

## Interruzione del demone

L'interruzione del demone attraverso `sshlirp_ci_stop` comporta automaticamente la terminazione del processo demone e l'eliminazione dei file temporanei creati durante l'esecuzione.
Per interrompere il demone è sufficiente lanciare il seguente comando, sostituendo `/path/to/sshlirpCI` con il percorso in cui è stato clonato il repository di sshlirpCI:

```sh
sudo /path/to/sshlirpCI/build/build/sshlirp_ci_stop
```
---
