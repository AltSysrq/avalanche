/* Compile with
 *
 *   javac JavaArrayList.java
 */
import java.util.*;

public class JavaArrayList {
  public static void main(String args[]) {
    List<Long> list = buildList(Integer.parseInt(args[0]));
    if (0 != Integer.parseInt(args[1]))
      sumList(list);
  }

  private static List<Long> buildList(int max) {
    List<Long> accum = new ArrayList<Long>();
    for (int i = 0; i < max; ++i)
      accum.add(Long.valueOf(i));

    return accum;
  }

  private static long sumList(List<Long> values) {
    long sum = 0;
    for (long l: values) sum += l;
    return sum;
  }
}
