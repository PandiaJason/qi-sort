import com.qisort.QiSort;
import java.util.Arrays;

public class TestJava {
    public static void main(String[] args) {
        System.out.println("Testing qi::sort in Java via JNI...");
        int[] data = {42, 10, 100, 5, 9999, 12};
        System.out.println("Before sorting: " + Arrays.toString(data));
        
        QiSort.sort(data);
        
        System.out.println("After sorting:  " + Arrays.toString(data));
        System.out.println("Java JNI qi::sort SUCCESS!");
    }
}
