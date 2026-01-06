import java.io.BufferedReader;
import java.io.InputStreamReader;

public class JoueurExterne {
    String name;
    JoueurExterne(String name){
        this.name=name;
    }

    public static void main(String[] args) throws Exception {
        JoueurExterne joueur=new JoueurExterne(args[0]);
        BufferedReader in = new BufferedReader(new InputStreamReader(System.in));
        String etat;

        while ((etat = in.readLine()) != null) {
            // ATTENTION PAS DE TRACE POSSIBLE !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            //System.out.println(joueur.name + ": lu=" + etat );
            if ("END".equals(etat)){
                System.out.println(joueur.name + ": Partie terminée");
                break;
            }
            String coup;
            if ("START".equals(etat)){
                //System.out.println(joueur.name + ": La partie commence");
                coup="1";
            } else {
                // On analyse
                int val = Integer.parseInt(etat);
                //System.out.println(joueur.name + ": valeur lue=" + val);
                // On calcule un coup et on le renvoie
                if (val==8) Thread.sleep(4000);
                if (val >= 20){ coup="RESULT " + val;}
                else {
                    coup = Integer.toString(val + 1);
                }
            }
            System.out.println(coup);
            System.out.flush();
        }
    }
}
