echo ""
echo "INICIANDO TESTE AUTOMÁTICO"
echo "============================================="
echo ""

# --- PASSO 1: PREPARAÇÃO DOS DIRETÓRIOS ---
echo "[PASSO 1/7] Preparando diretórios para a rede de 3 peers..."
# Garante que o ambiente está limpo
rm -rf peer1 peer2 peer3 peer4
# Cria as pastas
mkdir peer1 peer2 peer3
# Copia o executável (ajuste o caminho se necessário)
cp ../peer peer1/
cp ../peer peer2/
cp ../peer peer3/
echo "Diretórios prontos."
echo ""

# --- PASSO 2: INICIAR A REDE DE 3 PEERS ---
echo "[PASSO 2/7] Iniciando os 3 peers em segundo plano..."
cd peer1
./peer 8001 127.0.0.1:8002 127.0.0.1:8003 > peer1.log 2>&1 &
PEER1_PID=$!
cd ..

cd peer2
./peer 8002 127.0.0.1:8001 127.0.0.1:8003 > peer2.log 2>&1 &
PEER2_PID=$!
cd ..

cd peer3
./peer 8003 127.0.0.1:8001 127.0.0.1:8002 > peer3.log 2>&1 &
PEER3_PID=$!
cd ..

echo "Peers iniciados com PIDs: $PEER1_PID, $PEER2_PID, $PEER3_PID"
echo "Aguardando 10 segundos para a estabilização da rede..."
sleep 10

# --- PASSO 3: TESTE DE ADIÇÃO DE ARQUIVO (3 PEERS) ---
FILENAME="teste_rede_pequena.txt"
echo ""
echo "[PASSO 3/7] Adicionando arquivo '$FILENAME' no peer1..."
echo "Conteúdo de teste para 3 VMs" > peer1/tmp/$FILENAME

echo "Aguardando 10 segundos para a sincronização..."
sleep 10

echo "Verificando se o arquivo foi sincronizado para peer2 e peer3:"
ls -l peer2/tmp/
ls -l peer3/tmp/
echo ""


# --- PASSO 4: PARAR A REDE DE 3 PEERS ---
echo "[PASSO 4/7] Encerrando a rede de 3 peers para adicionar um novo nodo..."
kill $PEER1_PID $PEER2_PID $PEER3_PID
sleep 3 # Pequena pausa para garantir que os processos terminaram
echo "Rede de 3 peers encerrada."
echo ""

# --- PASSO 5: PREPARAR E INICIAR A REDE DE 4 PEERS ---
echo "[PASSO 5/7] Preparando e iniciando a nova rede com 4 peers..."
# Prepara o diretório do peer4
mkdir peer4
cp ../peer peer4/

# Inicia os 4 peers, agora todos se conhecem
cd peer1
./peer 8001 127.0.0.1:8002 127.0.0.1:8003 127.0.0.1:8004 > peer1_new.log 2>&1 &
PEER1_PID=$!
cd ..

cd peer2
./peer 8002 127.0.0.1:8001 127.0.0.1:8003 127.0.0.1:8004 > peer2_new.log 2>&1 &
PEER2_PID=$!
cd ..

cd peer3
./peer 8003 127.0.0.1:8001 127.0.0.1:8002 127.0.0.1:8004 > peer3_new.log 2>&1 &
PEER3_PID=$!
cd ..

cd peer4
./peer 8004 127.0.0.1:8001 127.0.0.1:8002 127.0.0.1:8003 > peer4.log 2>&1 &
PEER4_PID=$!
cd ..

echo "Nova rede de 4 peers iniciada."
echo "Aguardando 15 segundos para a estabilização da nova rede..."
sleep 15


# --- PASSO 6: TESTE DE SINCRONIZAÇÃO (4 PEERS) ---
FILENAME_NEW="teste_rede_grande.txt"
echo ""
echo "[PASSO 6/7] Adicionando arquivo '$FILENAME_NEW' no peer1 para testar o peer4..."
echo "Conteúdo para a rede de 4 peers" > peer1/tmp/$FILENAME_NEW

echo "Aguardando 10 segundos para a sincronização..."
sleep 10

echo "Verificando se o arquivo foi sincronizado para o novo peer4:"
ls -l peer4/tmp/
echo "O arquivo '$FILENAME_NEW' deve estar listado acima."
echo ""

# --- PASSO 7: LIMPEZA FINAL ---
echo "[PASSO 7/7] Encerrando os processos dos 4 peers..."
kill $PEER1_PID $PEER2_PID $PEER3_PID $PEER4_PID
# Remove os diretórios de teste
cd ..
rm -rf teste_localhost
echo "Processos encerrados e diretórios de teste limpos."
echo ""
echo TESTE AUTOMÁTICO CONCLUÍDO"