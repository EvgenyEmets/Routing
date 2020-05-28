# Routing
Routing App for RUNOS controller
Для работы приложения нужно установить приложение HostManager (https://github.com/ARCCN/host-manager.git)

        $ cd /runos/src/apps
        $ git clone https://github.com/ARCCN/host-manager.git
1) Установка приложения
    Чтобы установить приложение нужно скачать исходники и поместить их в директорию /runos/src/apps
    
        $ cd /runos/src/apps
        $ git clone https://github.com/EvgenyEmets/Routing.git
2) Сборка RUNOS

        $ cd ../..
        $ nix-shell
        $ mkdir build
        $ cd build
        $ cmake ..
        $ make
        $ cd ..

3) Запуск контроллера
        
        $ ./build/runos
