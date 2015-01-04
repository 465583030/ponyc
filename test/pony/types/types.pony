use "files"

trait T1
  fun tag t1(env: Env) =>
    env.out.print("T1")

trait T2
  fun tag t2(env: Env) =>
    env.out.print("T2")

class Wombat is T1, T2

actor Main
  new create(env: Env) =>
    let wombat: (T1 & T2) = Wombat
      wombat.t1(env)
      wombat.t2(env)

    let writer = object
      fun tag apply(out: File, matnr: String, quantity: F64, unit: String) =>
        out.write(matnr + "\t" + quantity.string() + "\t" + unit)
    end
