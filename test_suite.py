import unittest
import os
import subprocess

class TestMyShell(unittest.TestCase):

    def setUp(self):
        # Crear archivo temporal para pruebas
        with open("test_file.txt", "w") as f:
            f.write("linea de prueba 1\nlinea de prueba 2\n")

    def tearDown(self):
        # Limpiar archivos generados por pruebas
        for fname in ["test_file.txt", "out_test.txt"]:
            if os.path.exists(fname):
                os.remove(fname)

    def run_shell_command(self, cmd_input):
        proc = subprocess.Popen(
            ["python3", "main.py"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        stdout, stderr = proc.communicate(input=cmd_input + "\nexit\n")
        return stdout, stderr, proc.returncode

    def test_01_external_command(self):
        stdout, stderr, code = self.run_shell_command("echo Hola_POSIX")
        self.assertIn("Hola_POSIX", stdout)

    def test_02_mywrite_and_mycat(self):
        self.run_shell_command('mywrite out_test.txt "Prueba automatizada"')
        stdout, stderr, _ = self.run_shell_command("mycat out_test.txt")
        self.assertIn("Prueba automatizada", stdout)

    def test_03_getenv(self):
        stdout, stderr, _ = self.run_shell_command("getenv USER")
        self.assertTrue(len(stdout.strip()) > 0)

    def test_04_invalid_command(self):
        stdout, stderr, _ = self.run_shell_command("comando_invalido_xyz")
        self.assertIn("comando no encontrado", stderr)

if __name__ == "__main__":
    unittest.main()