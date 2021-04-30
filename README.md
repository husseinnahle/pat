## Description

```
pat [-s séparateur] commande arguments [+ commande arguments]...
```

Dans une session interactive dans un terminal, les sorties standard et d'erreurs des commandes sont affichées en vrac sur le terminal ce qui dans certains cas rend l'affichage confus et difficile à interpréter.

pat, pour *process cat* (ou *process and tail*) permet de distinguer les différentes sorties des commandes en insérant des lignes de séparation.

Par exemple, le programme `prog1` mélange sortie standard et sortie d'erreur de façon indiscernable.

```
$ ./prog1
Bonjour
Message d'erreur
Le
Monde
Message d'erreur de fin
```

Avec pat, des lignes de séparation sont affichées quand c'est une sortie différente qui utilisée.

```
$ pat ./prog1
+++ stdout
Bonjour
+++ stderr
Message d'erreur
+++ stdout
Le
Monde
+++ stderr
Message d'erreur de fin
+++ exit, status=0
```

Par défaut, quand il n'y a qu'une seule commande, le format des lignes de séparation est:

* l'indicateur de séparation, par défaut c'est `+++`;
* un espace
* `stdout` ou `stderr` pour distinguer la sortie

Lorsque la même sortie est réutilisée (même après un long laps de temps), aucune ligne de séparation n'est bien sûr réaffichée.

```
$ pat cat prog1
+++ stdout
#!/bin/sh
echo "Bonjour"
sleep .2
echo "Message d'erreur" >&2
sleep .2
echo "Le"
sleep .2
echo "Monde"
sleep .2
echo "Message d'erreur de fin" >&2
+++ exit, status=0
```

Lorsque la commande se termine, une ligne de séparation spéciale est affichée qui indique `exit`. Voir plus loin pour les détails.


### Lignes non terminées

La confusion entre les différentes sorties est exacerbée lorsque les lignes affichées ne sont pas terminées.

```
$ ./prog2
HelloFirst errorWorld2nd WorldLast error
```

pat permet de distinguer simplement et de façon non abiguë si un changement de sortie survient alors qu'une fin de ligne n'a pas été encore affichée.

```
$ pat ./prog2
+++ stdout
Hello
++++ stderr
First error
++++ stdout
World2nd World
++++ stderr
Last error
++++ exit, status=1
```

Ainsi, si la dernière ligne n'est pas terminée par un caractère de saut de ligne, un est inséré et l'indicateur de séparation est `++++` (quatre au lieu du trois).

### Terminaison de commande

Quand une commande termine de façon volontaire, la ligne de terminaison indique son code de retour avec « `, status=` ».
Quand une commande termine de façon involontaire (par un signal), la ligne de terminaison indique le numéro du signal avec « `, signal=` ».

```
$ pat ./prog3
+++ stdout
killme
+++ exit, signal=15
```

Si une commande ne peut pas s'exécuter (fichier invalide, etc.) un message d'erreur est affiché et la commande se termine avec un code de retour de 127.

```
$ pat fail
+++ stderr
fail: No such file or directory
+++ exit, status=127
```


### Exécution de plusieurs commandes

pat patrouille l'exécution de plusieurs commandes et distingue chacune leurs sorties.

L'argument « `+` » (tout seul) permet de séparer les commandes.
Un nombre quelconque de commandes peuvent donc ainsi être surveillées.

```
$ pat ./prog1 + ./prog2 + ./prog3
+++ stdout 1
Bonjour
+++ stdout 2
Hello
++++ stderr 1
Message d'erreur
+++ stderr 2
First error
++++ stdout 3
killme
+++ exit 3, signal=15
+++ stdout 1
Le
+++ stdout 2
World
++++ stdout 1
Monde
+++ stdout 2
2nd World
++++ stderr 1
Message d'erreur de fin
+++ exit 1, status=0
+++ stderr 2
Last error
++++ exit 2, status=1
```

Quand il y a plusieurs commandes, les lignes de séparation indiquent le numéro de la commande concernée. Le numéro de la commande commence à 1.

### Option -s

L'option `-s` permet de préciser le séparateur de commande utilisé dans la ligne de commande ainsi que l'indicateur utilisé dans les lignes de séparation.
Par défaut c'est `+` mais toute chaine valide peut être utilisée.

```
$ pat -s @@ ./prog2
@@@@@@ stdout
Hello
@@@@@@@@ stderr
First error
@@@@@@@@ stdout
World2nd World
@@@@@@@@ stderr
Last error
@@@@@@@@ exit, status=1
```


Changer le séparateur par défaut permet d'utiliser `pat` si jamais `+` a un sens particulier dans les arguments des commandes ou si l'indicateur `+++` au début des lignes a un sens particulier dans les sorties des commandes (c'est le cas de `diff` par exemple).

En particulier, il permet a `pat` de patrouiller `pat` !

```
$ pat -s @1 pat -s @2 ./prog1 @2 ./prog2 @1 ./prog3
@1@1@1 stdout 1
@2@2@2 stdout 1
Bonjour
@2@2@2 stdout 2
Hello
@2@2@2@2 stderr 1
Message d'erreur
@2@2@2 stderr 2
First error
@1@1@1@1 stdout 2
killme
@1@1@1 exit 2, signal=15
@1@1@1 stdout 1

@2@2@2@2 stdout 1
Le
@2@2@2 stdout 2
World
@2@2@2@2 stdout 1
Monde
@2@2@2 stdout 2
2nd World
@2@2@2@2 stderr 1
Message d'erreur de fin
@2@2@2 exit 1, status=0
@2@2@2 stderr 2
Last error
@2@2@2@2 exit 2, status=1
@1@1@1 exit 1, status=1
```

