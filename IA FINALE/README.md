# Projet IA - Jeu de Stratégie (Variante Mancala/Awalé)

Ce projet implémente une Intelligence Artificielle performante capable de jouer à un jeu de stratégie complexe impliquant des graines de couleurs (Rouge, Bleu, Transparent). Le projet inclut également un Arbitre en Java pour gérer les matchs entre deux exécutables.

## 🚀 Fonctionnalités de l'IA

L'IA est développée en **C++17** pour maximiser les performances. Elle utilise des techniques avancées de recherche arborescente :

- **Algorithme principal** : Minimax avec élagage Alpha-Beta.
- **Recherche** :
  - _Iterative Deepening_ (Approfondissement itératif) pour respecter la limite de temps stricte (3 secondes).
  - _Principal Variation Search (PVS)_ pour optimiser l'ordre des nœuds explorés.
- **Optimisations** :
  - **Table de Transposition** avec Zobrist Hashing (pour ne pas recalculer les positions déjà vues).
  - **Move Ordering** : Utilisation de _Killer Moves_ et _History Heuristic_ pour tester les meilleurs coups en premier.
  - **Gestion mémoire** : Utilisation de vecteurs statiques (`StaticVector`) pour éviter les allocations dynamiques coûteuses pendant la recherche.
- **Fonction d'évaluation (BotDNA)** : Prise en compte du score, de la mobilité, du contrôle des trous, et pénalités pour la thésaurisation de graines.

## 🛠️ Compilation

### Pré-requis

- Compilateur C++ (G++ supportant C++17)
- Java Development Kit (JDK 21 ou supérieur)

### 1. Compiler l'IA (Joueur)

Pour générer l'exécutable portable (`player.exe`) sans dépendances DLL :

```bash
g++ Main.cpp -O3 -std=c++17 -static -static-libgcc -static-libstdc++ -o player.exe
```

### 2. Compiler l'Arbitre

```bash
javac Arbitre.java
```

## 🎮 Exécution

Pour lancer un match entre deux instances de votre IA :

```bash
java Arbitre
```

Par défaut, l'arbitre cherche `player.exe` dans le dossier courant. Vous pouvez spécifier deux exécutables différents :

```bash
java Arbitre ./mon_ia.exe ./autre_ia.exe
```

## 📂 Structure du Projet

- **C++ (IA)**

  - `Main.cpp` : Point d'entrée, gestion du protocole de communication (START, RESULT, parsing des coups).
  - `Search.hpp` : Cœur de l'IA (Algorithmes Minimax, PVS, Transposition Table).
  - `GameRules.hpp` : Logique du jeu (semaille, captures, déplacements).
  - `GameState.hpp` : Représentation optimisée du plateau (tableau 1D).
  - `Move.hpp` : Structure de données pour les coups.
  - `SearchStats.hpp` : (Optionnel) Structures pour les statistiques de recherche.

- **Java (Arbitre)**
  - `Arbitre.java` : Gestionnaire de partie, validation des coups, affichage du plateau et logs.

## 🧠 Détails de la Stratégie

L'IA utilise une approche "Anytime" grâce à l'approfondissement itératif. Elle renvoie toujours le meilleur coup trouvé à la profondeur `d` si le calcul pour `d+1` dépasse le temps imparti (2.8s de marge de sécurité).

La fonction d'évaluation pondère :

1.  **Différence de score** (Priorité absolue).
2.  **Matériel** : Nombre de graines possédées.
3.  **Position** : Contrôle des trous stratégiques.
4.  **Mobilité** : Capacité à jouer plusieurs coups différents.
5.  **Défense** : Évite de laisser des trous prenables (2 ou 3 graines).

## 📝 Auteurs
Cazacu Ion
Virgile Lassagne

Master Informatique
Projet réalisé dans le cadre du cours d'IA Game Programming - Janvier 2026.
