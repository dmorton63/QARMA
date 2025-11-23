"""
Interpreter for NOMAIN v0.1
Executes the Abstract Syntax Tree (AST)
"""

from ast_nodes import *
from token_types import TokenType


class RuntimeError(Exception):
    """Raised when interpreter encounters a runtime error"""
    pass


class ReturnValue(Exception):
    """Used to handle RETURN statements (not really an error)"""
    def __init__(self, value):
        self.value = value


class Interpreter:
    """Executes NOMAIN AST"""
    
    def __init__(self):
        self.globals = {}         # Global variables
        self.functions = {}       # Function definitions
        self.call_stack = []      # For local variables in function calls
    
    def run(self, program):
        """Execute a NOMAIN program"""
        # Initialize global variables
        for decl in program.data_storage.declarations:
            if decl.initial_value:
                value = self.eval_expression(decl.initial_value, {})
            else:
                value = self.get_default_value(decl.var_type)
            
            self.globals[decl.name] = {
                'type': decl.var_type,
                'value': value,
                'is_const': decl.is_const
            }
        
        # Store function definitions
        for func in program.functions:
            self.functions[func.name] = func
        
        # Find and execute startup function
        if program.startup_function not in self.functions:
            raise RuntimeError(f"Startup function '{program.startup_function}' not found")
        
        startup = self.functions[program.startup_function]
        self.call_function(startup, [])
    
    def get_default_value(self, var_type):
        """Get default value for a type"""
        defaults = {
            'INT': 0,
            'FLOAT': 0.0,
            'STRING': '',
            'BOOL': False
        }
        return defaults.get(var_type, None)
    
    def get_locals(self):
        """Get current local variables (top of call stack)"""
        if self.call_stack:
            return self.call_stack[-1]
        return {}
    
    def call_function(self, func_def, arguments):
        """Execute a function"""
        # Create local scope
        locals = {}
        
        # Bind parameters
        if len(arguments) != len(func_def.parameters):
            raise RuntimeError(f"Function '{func_def.name}' expects {len(func_def.parameters)} arguments, got {len(arguments)}")
        
        for param, arg_value in zip(func_def.parameters, arguments):
            locals[param.name] = {
                'type': param.var_type,
                'value': arg_value
            }
        
        # Push local scope onto call stack
        self.call_stack.append(locals)
        
        try:
            # Execute function body
            for statement in func_def.body:
                self.execute_statement(statement)
            
            # Pop local scope
            self.call_stack.pop()
            
            # Void function returns None
            return None
            
        except ReturnValue as ret:
            # Pop local scope
            self.call_stack.pop()
            return ret.value
    
    def execute_statement(self, stmt):
        """Execute a statement"""
        if isinstance(stmt, AssignmentStmt):
            self.execute_assignment(stmt)
        
        elif isinstance(stmt, ReturnStmt):
            value = None
            if stmt.value:
                value = self.eval_expression(stmt.value, self.get_locals())
            raise ReturnValue(value)
        
        elif isinstance(stmt, IfStmt):
            self.execute_if(stmt)
        
        elif isinstance(stmt, WhileStmt):
            self.execute_while(stmt)
        
        elif isinstance(stmt, ForStmt):
            self.execute_for(stmt)
        
        elif isinstance(stmt, VarDeclStmt):
            self.execute_var_decl(stmt)
        
        elif isinstance(stmt, ExpressionStmt):
            self.eval_expression(stmt.expression, self.get_locals())
        
        else:
            raise RuntimeError(f"Unknown statement type: {type(stmt)}")
    
    def execute_assignment(self, stmt):
        """Execute assignment statement"""
        locals = self.get_locals()
        value = self.eval_expression(stmt.value, locals)
        
        # Check if it's a local variable
        if stmt.variable in locals:
            locals[stmt.variable]['value'] = value
        # Check if it's a global variable
        elif stmt.variable in self.globals:
            if self.globals[stmt.variable]['is_const']:
                raise RuntimeError(f"Cannot assign to CONST variable '{stmt.variable}'")
            self.globals[stmt.variable]['value'] = value
        else:
            raise RuntimeError(f"Variable '{stmt.variable}' not defined")
    
    def execute_if(self, stmt):
        """Execute IF statement"""
        locals = self.get_locals()
        condition = self.eval_expression(stmt.condition, locals)
        
        if condition:
            for s in stmt.then_block:
                self.execute_statement(s)
        elif stmt.else_block:
            for s in stmt.else_block:
                self.execute_statement(s)
    
    def execute_while(self, stmt):
        """Execute WHILE loop"""
        locals = self.get_locals()
        
        while self.eval_expression(stmt.condition, locals):
            for s in stmt.body:
                self.execute_statement(s)
    
    def execute_for(self, stmt):
        """Execute FOR loop"""
        # Execute initialization
        self.execute_statement(stmt.init)
        
        locals = self.get_locals()
        
        # Loop
        while self.eval_expression(stmt.condition, locals):
            # Execute body
            for s in stmt.body:
                self.execute_statement(s)
            
            # Execute increment
            self.execute_statement(stmt.increment)
    
    def execute_var_decl(self, stmt):
        """Execute local variable declaration"""
        locals = self.get_locals()
        
        if stmt.initial_value:
            value = self.eval_expression(stmt.initial_value, locals)
        else:
            value = self.get_default_value(stmt.var_type)
        
        locals[stmt.name] = {
            'type': stmt.var_type,
            'value': value
        }
    
    def eval_expression(self, expr, locals):
        """Evaluate an expression"""
        if isinstance(expr, Literal):
            return expr.value
        
        elif isinstance(expr, Variable):
            # Check locals first
            if expr.name in locals:
                return locals[expr.name]['value']
            # Then globals
            elif expr.name in self.globals:
                return self.globals[expr.name]['value']
            else:
                raise RuntimeError(f"Variable '{expr.name}' not defined")
        
        elif isinstance(expr, BinaryOp):
            return self.eval_binary_op(expr, locals)
        
        elif isinstance(expr, UnaryOp):
            return self.eval_unary_op(expr, locals)
        
        elif isinstance(expr, FunctionCall):
            return self.eval_function_call(expr, locals)
        
        else:
            raise RuntimeError(f"Unknown expression type: {type(expr)}")
    
    def eval_binary_op(self, expr, locals):
        """Evaluate binary operation"""
        left = self.eval_expression(expr.left, locals)
        right = self.eval_expression(expr.right, locals)
        op = expr.operator
        
        # Arithmetic
        if op == TokenType.PLUS:
            return left + right
        elif op == TokenType.MINUS:
            return left - right
        elif op == TokenType.MULTIPLY:
            return left * right
        elif op == TokenType.DIVIDE:
            if right == 0:
                raise RuntimeError("Division by zero")
            return left / right
        
        # Comparison
        elif op == TokenType.EQ:
            return left == right
        elif op == TokenType.NE:
            return left != right
        elif op == TokenType.LT:
            return left < right
        elif op == TokenType.GT:
            return left > right
        elif op == TokenType.LE:
            return left <= right
        elif op == TokenType.GE:
            return left >= right
        
        # Logical
        elif op == TokenType.AND:
            return left and right
        elif op == TokenType.OR:
            return left or right
        
        else:
            raise RuntimeError(f"Unknown binary operator: {op}")
    
    def eval_unary_op(self, expr, locals):
        """Evaluate unary operation"""
        operand = self.eval_expression(expr.operand, locals)
        op = expr.operator
        
        if op == TokenType.NOT:
            return not operand
        elif op == TokenType.MINUS:
            return -operand
        else:
            raise RuntimeError(f"Unknown unary operator: {op}")
    
    def eval_function_call(self, expr, locals):
        """Evaluate function call"""
        # Check for built-in I/O functions
        if expr.name == 'say':
            return self.builtin_say(expr, locals)
        elif expr.name == 'say_f':
            return self.builtin_say_f(expr, locals)
        elif expr.name == 'INPUT':
            return self.builtin_input(expr, locals)
        elif expr.name == 'INPUT_INT':
            return self.builtin_input_int(expr, locals)
        elif expr.name == 'INPUT_FLOAT':
            return self.builtin_input_float(expr, locals)
        
        # Check for built-in quantum functions
        elif expr.name == 'CreateQubit':
            return self.builtin_create_qubit(expr, locals)
        elif expr.name == 'CreateQubits':
            return self.builtin_create_qubits(expr, locals)
        elif expr.name == 'ApplyGate':
            return self.builtin_apply_gate(expr, locals)
        elif expr.name == 'Measure':
            return self.builtin_measure(expr, locals)
        elif expr.name == 'GetState':
            return self.builtin_get_state(expr, locals)
        
        # Check for visualization functions
        elif expr.name == 'PLOT_BLOCH_SPHERE':
            return self.builtin_plot_bloch_sphere(expr, locals)
        elif expr.name == 'PLOT_PROBABILITIES':
            return self.builtin_plot_probabilities(expr, locals)
        elif expr.name == 'SHOW_ENTANGLEMENT':
            return self.builtin_show_entanglement(expr, locals)
        
        # User-defined function
        if expr.name not in self.functions:
            raise RuntimeError(f"Function '{expr.name}' not defined")
        
        # Evaluate arguments
        arg_values = []
        for arg_expr in expr.arguments:
            arg_values.append(self.eval_expression(arg_expr, locals))
        
        # Call function
        func = self.functions[expr.name]
        return self.call_function(func, arg_values)
    
    # ========================================================================
    # Built-in Functions
    # ========================================================================
    
    def builtin_say(self, expr, locals):
        """Built-in say() function - print with newline"""
        if len(expr.arguments) != 1:
            raise RuntimeError("say() expects exactly 1 argument")
        
        value = self.eval_expression(expr.arguments[0], locals)
        print(value)
        return None
    
    def builtin_say_f(self, expr, locals):
        """Built-in say_f() function - print multiple values on one line"""
        values = []
        for arg in expr.arguments:
            value = self.eval_expression(arg, locals)
            values.append(str(value))
        
        print(''.join(values))
        return None
    
    def builtin_input(self, expr, locals):
        """Built-in INPUT() function - read string from user"""
        if len(expr.arguments) != 1:
            raise RuntimeError("INPUT() expects exactly 1 argument (prompt)")
        
        prompt = self.eval_expression(expr.arguments[0], locals)
        return input(str(prompt))
    
    def builtin_input_int(self, expr, locals):
        """Built-in INPUT_INT() function - read integer from user"""
        if len(expr.arguments) != 1:
            raise RuntimeError("INPUT_INT() expects exactly 1 argument (prompt)")
        
        prompt = self.eval_expression(expr.arguments[0], locals)
        while True:
            try:
                return int(input(str(prompt)))
            except ValueError:
                print("Invalid input. Please enter an integer.")
    
    def builtin_input_float(self, expr, locals):
        """Built-in INPUT_FLOAT() function - read float from user"""
        if len(expr.arguments) != 1:
            raise RuntimeError("INPUT_FLOAT() expects exactly 1 argument (prompt)")
        
        prompt = self.eval_expression(expr.arguments[0], locals)
        while True:
            try:
                return float(input(str(prompt)))
            except ValueError:
                print("Invalid input. Please enter a number.")
    
    # ========================================================================
    # Quantum Functions
    # ========================================================================
    
    def builtin_create_qubit(self, expr, locals):
        """Built-in CreateQubit() function - create a single qubit"""
        from quantum_lib import builtin_create_qubit
        
        if len(expr.arguments) == 0:
            return builtin_create_qubit()
        elif len(expr.arguments) == 2:
            alpha = self.eval_expression(expr.arguments[0], locals)
            beta = self.eval_expression(expr.arguments[1], locals)
            return builtin_create_qubit(alpha, beta)
        else:
            raise RuntimeError("CreateQubit() expects 0 or 2 arguments (alpha, beta)")
    
    def builtin_create_qubits(self, expr, locals):
        """Built-in CreateQubits() function - create quantum register"""
        from quantum_lib import builtin_create_qubits
        
        if len(expr.arguments) != 1:
            raise RuntimeError("CreateQubits() expects 1 argument (num_qubits)")
        
        num = self.eval_expression(expr.arguments[0], locals)
        return builtin_create_qubits(int(num))
    
    def builtin_apply_gate(self, expr, locals):
        """Built-in ApplyGate() function - apply quantum gate"""
        from quantum_lib import builtin_apply_gate
        
        if len(expr.arguments) < 2 or len(expr.arguments) > 3:
            raise RuntimeError("ApplyGate() expects 2-3 arguments (gate_name, qubit, [control])")
        
        gate_name = self.eval_expression(expr.arguments[0], locals)
        qubit = self.eval_expression(expr.arguments[1], locals)
        control = self.eval_expression(expr.arguments[2], locals) if len(expr.arguments) == 3 else None
        
        return builtin_apply_gate(str(gate_name), qubit, control)
    
    def builtin_measure(self, expr, locals):
        """Built-in Measure() function - measure qubit"""
        from quantum_lib import builtin_measure
        
        if len(expr.arguments) != 1:
            raise RuntimeError("Measure() expects 1 argument (qubit)")
        
        qubit = self.eval_expression(expr.arguments[0], locals)
        return builtin_measure(qubit)
    
    def builtin_get_state(self, expr, locals):
        """Built-in GetState() function - get state string"""
        from quantum_lib import builtin_get_state
        
        if len(expr.arguments) != 1:
            raise RuntimeError("GetState() expects 1 argument (qubit)")
        
        qubit = self.eval_expression(expr.arguments[0], locals)
        return builtin_get_state(qubit)
    
    # ========================================================================
    # Visualization Functions
    # ========================================================================
    
    def builtin_plot_bloch_sphere(self, expr, locals):
        """Built-in PLOT_BLOCH_SPHERE() function"""
        from visualization import builtin_plot_bloch_sphere
        
        if len(expr.arguments) != 1:
            raise RuntimeError("PLOT_BLOCH_SPHERE() expects 1 argument (qubit)")
        
        qubit = self.eval_expression(expr.arguments[0], locals)
        return builtin_plot_bloch_sphere(qubit)
    
    def builtin_plot_probabilities(self, expr, locals):
        """Built-in PLOT_PROBABILITIES() function"""
        from visualization import builtin_plot_probabilities
        
        if len(expr.arguments) != 1:
            raise RuntimeError("PLOT_PROBABILITIES() expects 1 argument (qubit)")
        
        qubit = self.eval_expression(expr.arguments[0], locals)
        return builtin_plot_probabilities(qubit)
    
    def builtin_show_entanglement(self, expr, locals):
        """Built-in SHOW_ENTANGLEMENT() function"""
        from visualization import builtin_show_entanglement
        
        if len(expr.arguments) != 1:
            raise RuntimeError("SHOW_ENTANGLEMENT() expects 1 argument (register)")
        
        register = self.eval_expression(expr.arguments[0], locals)
        return builtin_show_entanglement(register)


def run_file(filename):
    """Parse and execute a NOMAIN file"""
    from parser import parse_file
    
    ast = parse_file(filename)
    interpreter = Interpreter()
    interpreter.run(ast)


def run_string(source):
    """Parse and execute NOMAIN source code"""
    from parser import parse_string
    
    ast = parse_string(source)
    interpreter = Interpreter()
    interpreter.run(ast)


if __name__ == '__main__':
    import sys
    
    if len(sys.argv) > 1:
        # Run file provided as argument
        filename = sys.argv[1]
        try:
            with open(filename, 'r') as f:
                code = f.read()
            print(f"Running {filename}...")
            print()
            run_string(code)
        except FileNotFoundError:
            print(f"Error: File '{filename}' not found")
            sys.exit(1)
        except (RuntimeError, Exception) as e:
            print(f"Runtime Error: {e}")
            import traceback
            traceback.print_exc()
            sys.exit(1)
    else:
        # Run built-in test
        test_code = '''
    [DATA_STORAGE]
    [GLOBAL]
        INT x = 10;
        INT y = 20;
        INT result;
    
    function add(INT a, INT b) -> INT {
        INT sum = a + b;
        RETURN sum;
    }
    
    function multiply(INT a, INT b) -> INT {
        RETURN a * b;
    }
    
    [APPLICATION-STARTUP]
    function main() -> [VOID] {
        say("NOMAIN v0.1 Interpreter Test");
        say("============================");
        
        result = add(x, y);
        say_f("x + y = ", result);
        
        result = multiply(x, y);
        say_f("x * y = ", result);
        
        INT local_var = 42;
        say_f("Local variable: ", local_var);
        
        IF result > 100 THEN
            say("Result is greater than 100!");
        ELSE:
            say("Result is not greater than 100");
        
        say("");
        say("Counting:");
        FOR INT i = 1; i <= 5; i = i + 1 {
            say_f("  Count: ", i);
        }
        
        say("");
        say("Done!");
    }
    '''
        
        try:
            print("Running NOMAIN program...")
            print()
            run_string(test_code)
        except (RuntimeError, Exception) as e:
            print(f"Runtime Error: {e}")
            import traceback
            traceback.print_exc()
