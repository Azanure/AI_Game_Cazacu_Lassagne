# 1. Version minimale de CMake requise
cmake_minimum_required(VERSION 3.10)

# 2. Nom du projet
project(ArenaBattleAI)

# 3. Standard C++ (C++17 est bien pour votre projet)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 4. Inclusion des dossiers (pour trouver vos .hpp)
# Cela dit à l'ordinateur : "Cherche les fichiers header dans src/"
include_directories(src)

# 5. Définition de l'exécutable
# On dit : "Crée 'arena' à partir de main.cpp"
# Note : Pas besoin de lister les .hpp ici, ni les fichiers dans v1/v2 car ils sont en "inline"
add_executable(arena src/main_bot.cpp)

# Si un jour vous avez des .cpp dans common (ex: GameState.cpp), décommentez ceci :
# add_executable(arena src/main.cpp src/common/GameState.cpp)