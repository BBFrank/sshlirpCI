# sshlirpCI

**Continuous Integration per il progetto sshlirp - Instant VPN**

## Introduzione

`sshlirpCI` è un sistema automatizzato di Continuous Integration pensato per il progetto sshlirp. Permette di avere binari sempre aggiornati all’ultima versione del codice sorgente e compilati per diverse architetture.

Il cuore del sistema è un demone che, a intervalli regolari, aggiorna il codice sorgente di sshlirp e ne effettua la cross-compilazione in parallelo per tutte le architetture specificate. Tutte le operazioni sono loggate su file per un monitoraggio dettagliato.

## Prerequisiti

### Qemu static user e debootstrap

Per la cross-compilazione vengono utilizzati `qemu-user-static` e `debootstrap`. Installali con:

```sh
sudo apt install debootstrap qemu-user-static
```

### Dipendenze

Sono richiesti anche:

```sh
sudo apt install build-essential cmake git
```

### Permessi

È necessario eseguire sshlirpCI con privilegi root (`sudo`). Per evitare richieste di password, aggiungi queste righe a `/etc/sudoers` (sostituisci `user` e il percorso):

```
user ALL=(root) NOPASSWD: /path/to/sshlirpCI/build/build/sshlirp_ci_start
user ALL=(root) NOPASSWD: /path/to/sshlirpCI/build/build/sshlirp_ci_stop
```

## Clone

Clona il repository:

```sh
git clone https://github.com/BBFrank/sshlirpCI.git
```

## Compilazione

```sh
cd sshlirpCI
mkdir build
cd build
cmake ..
make
```

Otterrai gli eseguibili:

- `sshlirp_ci_start`: avvia il demone
- `sshlirp_ci_stop`: termina il demone e pulisce i file temporanei

## Avvio del demone

Per avviare il demone:

```sh
sudo /path/to/sshlirpCI/build/build/sshlirp_ci_start
```

## Monitoring del demone - Log files

Durante l’esecuzione, ogni thread e il processo principale scrivono log separati. I log principali vengono poi uniti a fine ciclo. Puoi monitorare:

- Log thread (host): nella directory `THREAD_LOG_DIR` (vedi `ci.conf`)
- Log thread (chroot): in `MAIN_DIR/${arch}-chroot/THREAD_CHROOT_LOG_FILE`
- Log principale: percorso definito in `ci.conf`
- Stato e PID: `/tmp/sshlirp_ci.state` e `/tmp/sshlirp_ci.pid`

## Interruzione del demone

Per fermare il demone e pulire i file temporanei:

```sh
sudo /path/to/sshlirpCI/build/build/sshlirp_ci_stop
```

## Configurazione

Tutte le variabili (architetture, directory, log, URL repo, intervallo di polling) sono definite nel file `ci.conf`. Modifica questo file per adattare sshlirpCI alle tue esigenze.

---
