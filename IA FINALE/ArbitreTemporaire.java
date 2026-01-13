import java.io.*;
import java.util.concurrent.*;

// CLASSE DUPLIQUÉE POUR GARDER UNE COPIE LOGGÉE
public class ArbitreTemporaire {
    private static final int TIMEOUT_SECONDS = 3;

    // Custom Logger pour écrire à la fois dans la console et dans un fichier
    static class TeeOutputStream extends OutputStream {
        private final OutputStream out1;
        private final OutputStream out2;

        public TeeOutputStream(OutputStream out1, OutputStream out2) {
            this.out1 = out1;
            this.out2 = out2;
        }

        @Override
        public void write(int b) throws IOException {
            out1.write(b);
            out2.write(b);
        }

        @Override
        public void write(byte[] b, int off, int len) throws IOException {
            out1.write(b, off, len);
            out2.write(b, off, len);
        }

        @Override
        public void flush() throws IOException {
            out1.flush();
            out2.flush();
        }

        @Override
        public void close() throws IOException {
            out1.close();
            out2.close();
        }
    }

    public static void main(String[] args) throws Exception {
        // --- MISE EN PLACE DU FICHIER LOG ---
        PrintStream originalOut = System.out;
        String logFileName = "partie_log.txt";
        FileOutputStream fileOut = new FileOutputStream(logFileName);
        TeeOutputStream tee = new TeeOutputStream(originalOut, fileOut);
        PrintStream multiOut = new PrintStream(tee);
        System.setOut(multiOut); // Redirection de System.out

        // --- DEBUT DU CODE ORIGINAL ADAPTÉ ---

        String commandA = args.length > 0 ? args[0] : ".\\player.exe";
        String commandB = args.length > 1 ? args[1] : ".\\player.exe";

        System.out.println("LOGGING ENABLED -> " + logFileName);
        System.out.println("Lancement de la partie:");
        System.out.println("J1: " + commandA);
        System.out.println("J2: " + commandB);

        Process A = Runtime.getRuntime().exec(commandA);
        Process B = Runtime.getRuntime().exec(commandB);

        Joueur joueurA = new Joueur("J1", A);
        Joueur joueurB = new Joueur("J2", B);

        Joueur courant = joueurA;
        Joueur autre = joueurB;
        int currentPlayerId = 1;

        GameState state = new GameState();
        String coup = "START";

        try {
            while (true) {
                courant.send(coup);
                String reponse = courant.getResponse(TIMEOUT_SECONDS);

                if (reponse == null) {
                    System.out.println(
                            "RESULT TIMEOUT " + state.scoreP1 + " " + state.scoreP2 + " (Joueur " + courant.nom + ")");
                    break;
                }

                Move move = Move.parse(reponse);
                if (move == null || !GameRules.isValidMove(state, move, currentPlayerId)) {
                    System.out.println("RESULT INVALID_MOVE " + state.scoreP1 + " " + state.scoreP2 + " (" + courant.nom
                            + " a joué " + reponse + ")");
                    break;
                }

                GameRules.applyMove(state, move, currentPlayerId);
                printBoard(state);

                System.out.println("JOUEUR " + courant.nom + " joue : " + reponse);
                System.out.println("RESULT " + reponse + " " + state.scoreP1 + " " + state.scoreP2);

                if (GameRules.isGameOver(state)) {
                    if (state.movesCount >= 400) {
                        System.out.println("RESULT LIMIT " + state.scoreP1 + " " + state.scoreP2);
                    }
                    try {
                        courant.send("RESULT " + state.scoreP1 + " " + state.scoreP2);
                        autre.send("RESULT " + state.scoreP1 + " " + state.scoreP2);
                    } catch (Exception e) {
                    }
                    break;
                }

                coup = reponse;
                Joueur tmp = courant;
                courant = autre;
                autre = tmp;
                currentPlayerId = (currentPlayerId == 1) ? 2 : 1;
            }
        } finally {
            joueurA.destroy();
            joueurB.destroy();
            multiOut.close(); // Ferme le flux
        }
    }

    static void printBoard(GameState state) {
        System.out.println("\n+---------------------------------------------------------------+");
        System.out.println(String.format("| MOVES: %-3d | SCORE J1: %-3d | SCORE J2: %-3d |  Total: %-3d |",
                state.movesCount, state.scoreP1, state.scoreP2, state.countAllSeedsOnBoard()));
        System.out.println("+---------------------------------------------------------------+");
        for (int i = 0; i < 16; i++) {
            String owner = (i % 2 == 0) ? "J1" : "J2";
            int r = state.getSeeds(i, 0);
            int b = state.getSeeds(i, 1);
            int t = state.getSeeds(i, 2);
            System.out.println(String.format("| %2d (%s) | R:%2d | B:%2d | T:%2d | Total:%2d |",
                    i + 1, owner, r, b, t, (r + b + t)));
        }
        System.out.println("+---------------------------------------------------------------+\n");
    }

    static class Joueur {
        String nom;
        Process process;
        BufferedWriter in;
        BufferedReader out;
        ExecutorService executor = Executors.newSingleThreadExecutor();

        Joueur(String nom, Process p) {
            this.nom = nom;
            this.process = p;
            this.in = new BufferedWriter(new OutputStreamWriter(p.getOutputStream()));
            this.out = new BufferedReader(new InputStreamReader(p.getInputStream()));
        }

        void send(String msg) throws IOException {
            in.write(msg);
            in.newLine();
            in.flush();
        }

        String getResponse(int timeoutSeconds) {
            Future<String> future = executor.submit(() -> out.readLine());
            try {
                return future.get(timeoutSeconds, TimeUnit.SECONDS);
            } catch (Exception e) {
                return null;
            }
        }

        void destroy() {
            executor.shutdownNow();
            process.destroy();
        }
    }

    static class GameState {
        static final int NB_HOLES = 16;
        static final int NB_COLORS = 3;
        byte[] board = new byte[NB_HOLES * NB_COLORS];
        int scoreP1 = 0;
        int scoreP2 = 0;
        int movesCount = 0;

        GameState() {
            for (int i = 0; i < board.length; i++)
                board[i] = 2;
        }

        int getSeeds(int hole, int color) {
            return board[hole * 3 + color] & 0xFF;
        }

        void setSeeds(int hole, int color, int val) {
            board[hole * 3 + color] = (byte) val;
        }

        void addSeeds(int hole, int color, int val) {
            board[hole * 3 + color] = (byte) (getSeeds(hole, color) + val);
        }

        void clearSeeds(int hole, int color) {
            setSeeds(hole, color, 0);
        }

        int countTotalSeeds(int hole) {
            return getSeeds(hole, 0) + getSeeds(hole, 1) + getSeeds(hole, 2);
        }

        int countAllSeedsOnBoard() {
            int sum = 0;
            for (int i = 0; i < NB_HOLES; i++)
                sum += countTotalSeeds(i);
            return sum;
        }
    }

    static class Move {
        int hole;
        int type;

        Move(int hole, int type) {
            this.hole = hole;
            this.type = type;
        }

        static Move parse(String s) {
            try {
                int i = 0;
                while (i < s.length() && Character.isDigit(s.charAt(i)))
                    i++;
                int hole = Integer.parseInt(s.substring(0, i)) - 1;
                String suffix = s.substring(i);
                int type = 0;
                if (suffix.equals("R"))
                    type = 0;
                else if (suffix.equals("B"))
                    type = 1;
                else if (suffix.equals("TR"))
                    type = 2;
                else if (suffix.equals("TB"))
                    type = 3;
                else
                    return null;

                if (hole < 0 || hole >= 16)
                    return null;
                return new Move(hole, type);
            } catch (Exception e) {
                return null;
            }
        }
    }

    static class GameRules {
        static final int RED = 0;
        static final int BLUE = 1;
        static final int TRANS = 2;

        static boolean isP1Hole(int hole) {
            return (hole % 2) == 0;
        }

        static boolean isCurrentPlayerHole(int hole, int playerId) {
            return (playerId == 1) ? isP1Hole(hole) : !isP1Hole(hole);
        }

        static boolean isValidMove(GameState state, Move move, int playerId) {
            if (!isCurrentPlayerHole(move.hole, playerId))
                return false;
            int r = state.getSeeds(move.hole, RED);
            int b = state.getSeeds(move.hole, BLUE);
            int t = state.getSeeds(move.hole, TRANS);
            if (move.type == 0 && r == 0)
                return false;
            if (move.type == 1 && b == 0)
                return false;
            if ((move.type == 2 || move.type == 3) && t == 0)
                return false;
            return true;
        }

        static boolean isGameOver(GameState state) {
            return state.scoreP1 >= 49 || state.scoreP2 >= 49 || state.movesCount >= 400
                    || state.countAllSeedsOnBoard() < 10;
        }

        static void applyMove(GameState state, Move move, int playerId) {
            int seedsTrans = 0, seedsColor = 0, colorPlayed = RED;
            if (move.type == 0) {
                seedsColor = state.getSeeds(move.hole, RED);
                state.clearSeeds(move.hole, RED);
                colorPlayed = RED;
            } else if (move.type == 1) {
                seedsColor = state.getSeeds(move.hole, BLUE);
                state.clearSeeds(move.hole, BLUE);
                colorPlayed = BLUE;
            } else if (move.type == 2) {
                seedsTrans = state.getSeeds(move.hole, TRANS);
                seedsColor = state.getSeeds(move.hole, RED);
                state.clearSeeds(move.hole, TRANS);
                state.clearSeeds(move.hole, RED);
                colorPlayed = RED;
            } else if (move.type == 3) {
                seedsTrans = state.getSeeds(move.hole, TRANS);
                seedsColor = state.getSeeds(move.hole, BLUE);
                state.clearSeeds(move.hole, TRANS);
                state.clearSeeds(move.hole, BLUE);
                colorPlayed = BLUE;
            }

            int currentHole = move.hole;
            for (int phase = 0; phase < 2; phase++) {
                int seedsToSow = (phase == 0) ? seedsTrans : seedsColor;
                int typeToSow = (phase == 0) ? TRANS : colorPlayed;
                while (seedsToSow > 0) {
                    currentHole = (currentHole + 1) % 16;
                    if (currentHole == move.hole)
                        continue;
                    if (colorPlayed == BLUE && isCurrentPlayerHole(currentHole, playerId))
                        continue;
                    state.addSeeds(currentHole, typeToSow, 1);
                    seedsToSow--;
                }
            }

            int captureHole = currentHole;
            boolean keepCapturing = true;
            int safeguard = 0;
            while (keepCapturing && safeguard < 35) {
                safeguard++;
                int total = state.countTotalSeeds(captureHole);
                if (total == 2 || total == 3) {
                    int captured = total;
                    state.clearSeeds(captureHole, RED);
                    state.clearSeeds(captureHole, BLUE);
                    state.clearSeeds(captureHole, TRANS);
                    if (playerId == 1)
                        state.scoreP1 += captured;
                    else
                        state.scoreP2 += captured;
                    captureHole = (captureHole - 1 + 16) % 16;
                } else {
                    keepCapturing = false;
                }
                if (captureHole == currentHole && keepCapturing)
                    break;
            }
            state.movesCount++;
        }
    }
}
