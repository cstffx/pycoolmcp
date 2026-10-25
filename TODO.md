# TODO

- Cuerpo por rango: en vez de body:true (método entero), un lines:[start,end] o max_lines. Casi siempre bastan 10-20 líneas; el body completo suele ser el 80% del coste.
- Limpiar la salida de query_class: quitar campos vacíos (bases=, docstring=, refs=n/a) y no repetir module=file; con full ya se duplica información.
- Tope duro + paginación: cortar a ~2-4 KB y añadir … (+N más; usa limit/offset). Evita que un full/refs gigante reviente el contexto.
- compact por defecto y json:false; deja full/body como opt-in explícito.
- Filtro obligatorio en nombres ambiguos: si name aparece en >N ficheros, devolver solo archivo:línea y pedir file antes de detallar.
- Esquema: 4 tools con ~12 params opcionales cuestan fijo en cada turno; una sola tool pycool(op, args) o quitar params que ya cubre query_class reduce ese fijo.
- Índice más pequeño: excluir tests/, vendor/, .venv del indexado → menos matches y menos ruido.
- Chequeo de schema_version al abrir (lo que ya hablamos): un error de esquema cuesta una llamada + un reintento.

- Add tests for overlap, ignore patterns, and incremental updates.
