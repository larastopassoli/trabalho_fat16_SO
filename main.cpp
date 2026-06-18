#include <iostream>
#include <fstream>
#include <string>

using namespace std;

void listarConteudoDisco() {
    cout << "\n[LISTAR CONTEUDO DO DISCO]\n";
    cout << "Funcao ainda sera implementada.\n";
}

void listarConteudoArquivo() {
    cout << "\n[LISTAR CONTEUDO DE UM ARQUIVO]\n";
    cout << "Funcao ainda sera implementada.\n";
}

void exibirAtributosArquivo() {
    cout << "\n[EXIBIR ATRIBUTOS DE UM ARQUIVO]\n";
    cout << "Funcao ainda sera implementada.\n";
}

void renomearArquivo() {
    cout << "\n[RENOMEAR ARQUIVO]\n";
    cout << "Funcao ainda sera implementada.\n";
}

void inserirArquivo() {
    cout << "\n[INSERIR/CRIAR NOVO ARQUIVO]\n";
    cout << "Funcao ainda sera implementada.\n";
}

void apagarArquivo() {
    cout << "\n[APAGAR/REMOVER ARQUIVO]\n";
    cout << "Funcao ainda sera implementada.\n";
}

int main() {
    string nomeImagem = "disco.img";

    fstream imagem(nomeImagem, ios::in | ios::out | ios::binary);

    if (!imagem.is_open()) {
        cout << "Erro: nao foi possivel abrir a imagem de disco: " << nomeImagem << endl;
        cout << "Verifique se o arquivo disco.img esta na mesma pasta do programa.\n";
        return 1;
    }

    cout << "Imagem de disco aberta com sucesso: " << nomeImagem << endl;

    int opcao;

    do {
        cout << "\n========== MANIPULADOR FAT16 ==========\n";
        cout << "1 - Listar conteudo do disco\n";
        cout << "2 - Listar conteudo de um arquivo\n";
        cout << "3 - Exibir atributos de um arquivo\n";
        cout << "4 - Renomear arquivo\n";
        cout << "5 - Inserir/criar novo arquivo\n";
        cout << "6 - Apagar/remover arquivo\n";
        cout << "0 - Sair\n";
        cout << "Escolha uma opcao: ";
        cin >> opcao;

        switch (opcao) {
            case 1:
                listarConteudoDisco();
                break;
            case 2:
                listarConteudoArquivo();
                break;
            case 3:
                exibirAtributosArquivo();
                break;
            case 4:
                renomearArquivo();
                break;
            case 5:
                inserirArquivo();
                break;
            case 6:
                apagarArquivo();
                break;
            case 0:
                cout << "Encerrando o programa...\n";
                break;
            default:
                cout << "Opcao invalida. Tente novamente.\n";
        }

    } while (opcao != 0);

    imagem.close();

    return 0;
}