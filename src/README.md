# Tema 1 APD

Pentru inceput, in functia `main()` se initializeaza datele necesare algoritmului si thread-urilor si se aloca memoria pentru acestea.

Pentru fiecare thread in parte se initializeaza structura de argumente creata special care contine pointeri la datele aferente algoritmului care urmeaza sa fie prelucrate in thread si alte date necesare paralelizarii (nr. thread-uri, ID thread curent, bariera).

Thread-urile executa aceiasi pasi ai algoritmului in aceeasi ordine, doar ca functiile corespunzatoare fiecarui pas au fost paralelizate dupa stilul abordat la laborator: blocurile `for {}` se paralelizeaza calculandu-se intervalul in care va lucra thread-ul respectiv, in functie de ID-ul thread-ului curent.

Intre anumiti pasi ai algoritmului s-a aplicat o bariera pentru a se asigura faptul ca pasul anterior a fost executat integral si toate datele sunt disponibile pentru toate thread-urile.

La final se scrie imaginea obtinuta in fisierul de output, acest lucru fiind facut doar de **thread-ul 0**.