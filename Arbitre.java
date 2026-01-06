import java.io.*;
import java.util.concurrent.*;
import java.util.*;

public class Arbitre {
    // --- CONFIGURATION ---
    private static final int TIMEOUT_SECONDS = 3;
    private static final String EXE_NAME_1 = "v6.exe";
    private static final String EXE_NAME_2 = "v8.exe";
    
    // --- CONSTANTES DU JEU ---
    private static final int NB_HOLES = 16;
    private static final int RED = 0;
    private static final int BLUE = 1;
    private static final int TRANS = 2;

    public static void main(String[] args) throws Exception {
        // 1. Initialisation des exécutables
        String currentDir = System.getProperty("user.dir");
        String exePath1 = currentDir + File.separator + EXE_NAME_1;
        String exePath2 = currentDir + File.separator + EXE_NAME_2;

        if (!new File(exePath1).exists()) { System.err.println("ERREUR: Manque " + EXE_NAME_1); return; }
        if (!new File(exePath2).exists()) { System.err.println("ERREUR: Manque " + EXE_NAME_2); return; }

        System.out.println("Lancement du tournoi : " + EXE_NAME_1 + " (P1) vs " + EXE_NAME_2 + " (P2)");

        ProcessBuilder pb1 = new ProcessBuilder(exePath1);
        pb1.redirectError(ProcessBuilder.Redirect.INHERIT);
        ProcessBuilder pb2 = new ProcessBuilder(exePath2);
        pb2.redirectError(ProcessBuilder.Redirect.INHERIT);

        Joueur joueurA = new Joueur("Bot_V6_P1", pb1.start());
        Joueur joueurB = new Joueur("Bot_V8_P2", pb2.start());

        // 2. Initialisation du plateau officiel (L'Arbitre est la vérité)
        GameState state = new GameState();

        Joueur courant = joueurA;
        Joueur autre = joueurB;
        String dernierCoup = "START";
        int nbCoups = 0;

        System.out.println("--- DEBUT DE LA PARTIE ---");
        System.out.println("Score initial: 0 - 0");

        // 3. Boucle de jeu
        while (true) {
            // A. Vérification des conditions de fin AVANT le coup
            if (state.isGameOver()) {
                System.out.println("FIN DE PARTIE (Condition atteinte).");
                break;
            }

            // B. Envoi au joueur
            try {
                courant.send(dernierCoup);
            } catch (IOException e) {
                disqualifier(courant, "Erreur communication"); break;
            }

            // C. Réception
            String reponse = courant.getResponse(TIMEOUT_SECONDS);
            if (reponse == null) {
                disqualifier(courant, "TIMEOUT"); break;
            }
            reponse = reponse.trim();
            nbCoups++;

            // D. Analyse et Validation du coup
            Move move = parseMove(reponse);
            if (move == null) {
                disqualifier(courant, "Coup invalide (Format incorrect): " + reponse);
                break;
            }

            // Vérification légale (Propriété du trou)
            // P1 joue les impairs (indices 0, 2, 4...) -> Trous 1, 3, 5...
            int playerId = (courant == joueurA) ? 1 : 2;
            if (!state.isPlayerHole(move.holeIdx, playerId)) {
                disqualifier(courant, "Coup illégal (Trou adverse): " + reponse);
                break;
            }
            if (state.isEmpty(move.holeIdx)) {
                 disqualifier(courant, "Coup illégal (Trou vide): " + reponse);
                 break;
            }

            // E. Application du coup sur le plateau officiel
            state.applyMove(move, playerId);

            // F. Affichage
            System.out.println(String.format("[%d] %s joue %s -> Score: %d - %d", 
                nbCoups, courant.nom, reponse, state.scoreP1, state.scoreP2));

            if (nbCoups >= 400) {
                System.out.println("LIMITE DE 400 COUPS ATTEINTE.");
                break;
            }

            dernierCoup = reponse;
            Joueur tmp = courant; courant = autre; autre = tmp;
        }

        // 4. Résultats Finaux
        try { joueurA.send("END"); joueurB.send("END"); } catch (Exception e) {}
        joueurA.destroy(); joueurB.destroy();

        System.out.println("\n--- RESULTATS FINAUX ---");
        System.out.println("Score P1 (" + joueurA.nom + ") : " + state.scoreP1);
        System.out.println("Score P2 (" + joueurB.nom + ") : " + state.scoreP2);
        
        if (state.scoreP1 > state.scoreP2) System.out.println(">> VAINQUEUR : " + joueurA.nom + " <<");
        else if (state.scoreP2 > state.scoreP1) System.out.println(">> VAINQUEUR : " + joueurB.nom + " <<");
        else System.out.println(">> MATCH NUL <<");
    }

    private static void disqualifier(Joueur j, String raison) {
        System.out.println("RESULT : " + j.nom + " DISQUALIFIE (" + raison + ")");
    }

    // --- PARSING ---
    // Transforme "12tb" ou "5r" en objet Move
// --- PARSING CORRIGÉ ---
    // Transforme "12tb" ou "0r" en objet Move
    // CORRECTION : On ne fait plus -1 car le C++ envoie déjà l'index 0-15
    private static Move parseMove(String s) {
        try {
            int i = 0;
            // On lit tous les chiffres au début de la chaine
            while (i < s.length() && Character.isDigit(s.charAt(i))) i++;
            
            if (i == 0) return null; // Pas de chiffres trouvés
            
            // On récupère l'index brut (ex: "0" reste 0)
            int hole = Integer.parseInt(s.substring(0, i)); 
            
            // Sécurité : Si jamais un bot envoie quand même 16, on le refuse
            if (hole < 0 || hole > 15) return null;

            String suffix = s.substring(i).toLowerCase();
            
            int type;
            if (suffix.equals("r")) type = 0; // RED
            else if (suffix.equals("b")) type = 1; // BLUE
            else if (suffix.equals("tr")) type = 2; // TRANS_RED
            else if (suffix.equals("tb")) type = 3; // TRANS_BLUE
            else return null;

            return new Move(hole, type);
        } catch (Exception e) { return null; }
    }

    // --- CLASSES INTERNES ---

    static class Move {
        int holeIdx;
        int type; // 0=R, 1=B, 2=TR, 3=TB
        Move(int h, int t) { this.holeIdx = h; this.type = t; }
    }

    static class GameState {
        int[][] board = new int[NB_HOLES][3]; // [Trou][Couleur]
        int scoreP1 = 0;
        int scoreP2 = 0;

        GameState() {
            for (int i = 0; i < NB_HOLES; i++) {
                board[i][RED] = 2;
                board[i][BLUE] = 2;
                board[i][TRANS] = 2;
            }
        }

        boolean isPlayerHole(int idx, int pid) {
            // P1 (impairs) -> indices 0, 2, 4... (pair)
            // P2 (pairs) -> indices 1, 3, 5... (impair)
            if (pid == 1) return (idx % 2) == 0;
            return (idx % 2) != 0;
        }

        boolean isEmpty(int idx) {
            return (board[idx][RED] + board[idx][BLUE] + board[idx][TRANS]) == 0;
        }

        int countTotalSeeds() {
            int total = 0;
            for(int i=0; i<NB_HOLES; i++) total += (board[i][0] + board[i][1] + board[i][2]);
            return total;
        }

        boolean isGameOver() {
            return scoreP1 >= 49 || scoreP2 >= 49 || countTotalSeeds() < 10;
        }

        void applyMove(Move m, int pid) {
            int seedsTrans = 0;
            int seedsColor = 0;
            int colorPlayed = RED; // 0=Red, 1=Blue

            // 1. Récolte
            if (m.type == 0) { // RED
                seedsColor = board[m.holeIdx][RED];
                board[m.holeIdx][RED] = 0;
                colorPlayed = RED;
            } else if (m.type == 1) { // BLUE
                seedsColor = board[m.holeIdx][BLUE];
                board[m.holeIdx][BLUE] = 0;
                colorPlayed = BLUE;
            } else if (m.type == 2) { // TRANS -> RED
                seedsTrans = board[m.holeIdx][TRANS];
                seedsColor = board[m.holeIdx][RED];
                board[m.holeIdx][TRANS] = 0;
                board[m.holeIdx][RED] = 0;
                colorPlayed = RED;
            } else if (m.type == 3) { // TRANS -> BLUE
                seedsTrans = board[m.holeIdx][TRANS];
                seedsColor = board[m.holeIdx][BLUE];
                board[m.holeIdx][TRANS] = 0;
                board[m.holeIdx][BLUE] = 0;
                colorPlayed = BLUE;
            }

            int currentHole = m.holeIdx;

            // 2. Semaille (2 phases)
            for (int phase = 0; phase < 2; phase++) {
                int seedsToSow = (phase == 0) ? seedsTrans : seedsColor;
                int typeToSow = (phase == 0) ? TRANS : colorPlayed;

                while (seedsToSow > 0) {
                    currentHole = (currentHole + 1) % NB_HOLES; // Sens horaire
                    if (currentHole == m.holeIdx) continue; // Sauter départ

                    // Règle Bleue : Sauter ses propres trous
                    if (colorPlayed == BLUE && isPlayerHole(currentHole, pid)) continue;

                    board[currentHole][typeToSow]++;
                    seedsToSow--;
                }
            }

            // 3. Capture (Sens anti-horaire)
            int capturePtr = currentHole;
            boolean capturing = true;
            int loopGuard = 0;

            while (capturing && loopGuard < NB_HOLES) {
                int tot = board[capturePtr][0] + board[capturePtr][1] + board[capturePtr][2];
                if (tot == 2 || tot == 3) {
                    int gain = tot;
                    board[capturePtr][0] = 0;
                    board[capturePtr][1] = 0;
                    board[capturePtr][2] = 0;
                    
                    if (pid == 1) scoreP1 += gain;
                    else scoreP2 += gain;

                    // Reculer
                    capturePtr = (capturePtr - 1 + NB_HOLES) % NB_HOLES;
                } else {
                    capturing = false;
                }
                loopGuard++;
            }
        }
    }

    static class Joueur {
        String nom;
        Process process;
        BufferedWriter writer;
        BufferedReader reader;
        ExecutorService executor = Executors.newSingleThreadExecutor();

        Joueur(String nom, Process p) {
            this.nom = nom;
            this.process = p;
            this.writer = new BufferedWriter(new OutputStreamWriter(p.getOutputStream()));
            this.reader = new BufferedReader(new InputStreamReader(p.getInputStream()));
        }

        void send(String msg) throws IOException {
            writer.write(msg);
            writer.newLine();
            writer.flush();
        }

        String getResponse(int timeoutSeconds) {
            Future<String> future = executor.submit(() -> reader.readLine());
            try {
                return future.get(timeoutSeconds, TimeUnit.SECONDS);
            } catch (Exception e) {
                return null;
            }
        }

        void destroy() {
            executor.shutdownNow();
            if (process.isAlive()) process.destroyForcibly();
        }
    }
}